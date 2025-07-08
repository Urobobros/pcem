#ifdef USE_WHPX
#include "whpx.h"
#include "x86.h"
#include "ibm.h"
#include "mem.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>

#ifdef __MINGW32__
/* MinGW headers expose segment attributes directly as a UINT16 field */
#define SEGATTR(seg) ((seg).Attributes)
#else
/* Windows SDK headers wrap the attributes inside a union */
#define SEGATTR(seg) ((seg).Attributes.AsUINT16)
#endif

/* Attribute values for real-mode segments */
#define WHPX_REAL_MODE_CODE_ATTR 0x009B /* present, exec/read, 16-bit */
#define WHPX_REAL_MODE_DATA_ATTR 0x0093 /* present, read/write, 16-bit */

struct whpx_hr_entry {
    HRESULT hr;
    const char *name;
};

static const struct whpx_hr_entry whpx_hr_table[] = {
#ifdef WHV_E_UNKNOWN_CAPABILITY
    {WHV_E_UNKNOWN_CAPABILITY, "WHV_E_UNKNOWN_CAPABILITY"},
#endif
#ifdef WHV_E_INSUFFICIENT_BUFFER
    {WHV_E_INSUFFICIENT_BUFFER, "WHV_E_INSUFFICIENT_BUFFER"},
#endif
#ifdef WHV_E_UNKNOWN_PROPERTY
    {WHV_E_UNKNOWN_PROPERTY, "WHV_E_UNKNOWN_PROPERTY"},
#endif
#ifdef WHV_E_UNSUPPORTED_HYPERVISOR_CONFIG
    {WHV_E_UNSUPPORTED_HYPERVISOR_CONFIG, "WHV_E_UNSUPPORTED_HYPERVISOR_CONFIG"},
#endif
#ifdef WHV_E_INVALID_PARTITION_CONFIG
    {WHV_E_INVALID_PARTITION_CONFIG, "WHV_E_INVALID_PARTITION_CONFIG"},
#endif
#ifdef WHV_E_GPA_RANGE_NOT_FOUND
    {WHV_E_GPA_RANGE_NOT_FOUND, "WHV_E_GPA_RANGE_NOT_FOUND"},
#endif
#ifdef WHV_E_VP_ALREADY_EXISTS
    {WHV_E_VP_ALREADY_EXISTS, "WHV_E_VP_ALREADY_EXISTS"},
#endif
#ifdef WHV_E_VP_DOES_NOT_EXIST
    {WHV_E_VP_DOES_NOT_EXIST, "WHV_E_VP_DOES_NOT_EXIST"},
#endif
#ifdef WHV_E_INVALID_VP_STATE
    {WHV_E_INVALID_VP_STATE, "WHV_E_INVALID_VP_STATE"},
#endif
#ifdef WHV_E_INVALID_VP_REGISTER_NAME
    {WHV_E_INVALID_VP_REGISTER_NAME, "WHV_E_INVALID_VP_REGISTER_NAME"},
#endif
#ifdef WHV_E_INVALID_ARG
    {WHV_E_INVALID_ARG, "WHV_E_INVALID_ARG"},
#endif
    {0, NULL}
};

static const char *whpx_hresult_name(HRESULT hr)
{
    for (int i = 0; whpx_hr_table[i].name; i++)
        if (whpx_hr_table[i].hr == hr)
            return whpx_hr_table[i].name;
    return NULL;
}

static void whpx_log_hresult(const char *prefix, HRESULT hr)
{
    char *msg = NULL;
    HMODULE mod = GetModuleHandleA("WinHvPlatform.dll");
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_FROM_HMODULE |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   mod, hr, 0, (LPSTR)&msg, 0, NULL);
    const char *name = whpx_hresult_name(hr);
    if (msg) {
        size_t len = strlen(msg);
        while (len && (msg[len - 1] == '\n' || msg[len - 1] == '\r'))
            msg[--len] = '\0';
        if (name)
            pclog("%s failed: 0x%lx (%s) - %s\n", prefix, hr, name, msg);
        else
            pclog("%s failed: 0x%lx - %s\n", prefix, hr, msg);
        LocalFree(msg);
    } else {
        if (name)
            pclog("%s failed: 0x%lx (%s)\n", prefix, hr, name);
        else
            pclog("%s failed: 0x%lx\n", prefix, hr);
    }
}

static WHV_PARTITION_HANDLE whpx_partition = NULL;
static UINT32 whpx_vcpu_id = 0;
static void *whpx_ram = NULL;
static size_t whpx_ram_size = 0;
static int whpx_vcpu_created = 0;
static int whpx_first_run = 1;

/* Helper for initializing segment register values */
static void init_segment(WHV_REGISTER_VALUE *segment,
                         uint16_t selector,
                         uint64_t base,
                         uint32_t limit,
                         uint16_t attributes)
{
    segment->Segment.Selector = selector;
    segment->Segment.Base = base;
    segment->Segment.Limit = limit;
    SEGATTR(segment->Segment) = attributes;
}

static void whpx_dump_vp_registers(const char *msg)
{
    WHV_REGISTER_NAME regs_to_dump[] = {
        WHvX64RegisterRip,
        WHvX64RegisterCs,
        WHvX64RegisterRflags,
        WHvX64RegisterCr0,
        WHvX64RegisterCr3,
        WHvX64RegisterCr4,
    };
    WHV_REGISTER_VALUE values[sizeof(regs_to_dump)/sizeof(regs_to_dump[0])];

    HRESULT hr = WHvGetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id,
        regs_to_dump, sizeof(regs_to_dump)/sizeof(regs_to_dump[0]), values);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvGetVirtualProcessorRegisters", hr);
        return;
    }

    pclog("whpx: %s register dump:\n", msg ? msg : "");
    pclog("  RIP=0x%016llX\n", values[0].Reg64);
    pclog("  CS selector=0x%04X base=0x%016llX limit=0x%08X attr=0x%04X\n",
          values[1].Segment.Selector, values[1].Segment.Base,
          values[1].Segment.Limit, SEGATTR(values[1].Segment));
    pclog("  EFLAGS=0x%08llX CR0=0x%016llX CR3=0x%016llX CR4=0x%016llX\n",
          values[2].Reg64, values[3].Reg64, values[4].Reg64,
          values[5].Reg64);
}

/*
 * Initialize VCPU registers for real-mode execution starting at the
 * BIOS reset vector F000:FFF0.  This ensures WHPX sees valid segment
 * descriptors and control register values when the CPU first runs.
 */
static int init_real_mode_registers(void)
{
    /* Initialize registers for real-mode boot at F000:FFF0 */
    WHV_REGISTER_NAME regs[] = {
        WHvX64RegisterRip,
        WHvX64RegisterRflags,
        WHvX64RegisterCr0,
        WHvX64RegisterCr3,
        WHvX64RegisterCr4,
        WHvX64RegisterEfer,
        WHvX64RegisterCs,
        WHvX64RegisterSs,
        WHvX64RegisterDs,
        WHvX64RegisterEs,
        WHvX64RegisterFs,
        WHvX64RegisterGs,
    };
    WHV_REGISTER_VALUE vals[sizeof(regs)/sizeof(regs[0])];

    const USHORT data_attr = WHPX_REAL_MODE_DATA_ATTR;
    const USHORT code_attr = WHPX_REAL_MODE_CODE_ATTR;

    memset(vals, 0, sizeof(vals));
    vals[0].Reg64 = 0xFFF0;        /* RIP */
    vals[1].Reg64 = 0x00000002;    /* RFLAGS */
    vals[2].Reg64 = 0x00000010;    /* CR0 real mode */
    vals[3].Reg64 = 0;             /* CR3 */
    vals[4].Reg64 = 0;             /* CR4 */
    vals[5].Reg64 = 0;             /* EFER */

    /* CS segment */
    init_segment(&vals[6], 0xF000, 0xF0000, 0xFFFF, code_attr);
    cpu_state.seg_cs.seg       = 0xF000;
    cpu_state.seg_cs.base      = 0xF0000;
    cpu_state.seg_cs.limit     = 0xFFFF;
    cpu_state.seg_cs.access    = code_attr & 0xFF;
    pclog("whpx: Setting CS: selector=0x%04X base=0x%08X attr=0x%04X\n",
          vals[6].Segment.Selector,
          (UINT32)vals[6].Segment.Base,
          SEGATTR(vals[6].Segment));

    /* Data segments */
    for (int i = 7; i < (int)(sizeof(regs)/sizeof(regs[0])); i++) {
        init_segment(&vals[i], 0, 0, 0xFFFF, data_attr);
        switch (i) {
        case 7: /* SS */
            cpu_state.seg_ss.seg    = 0;
            cpu_state.seg_ss.base   = 0;
            cpu_state.seg_ss.limit  = 0xFFFF;
            cpu_state.seg_ss.access = data_attr & 0xFF;
            break;
        case 8: /* DS */
            cpu_state.seg_ds.seg    = 0;
            cpu_state.seg_ds.base   = 0;
            cpu_state.seg_ds.limit  = 0xFFFF;
            cpu_state.seg_ds.access = data_attr & 0xFF;
            break;
        case 9: /* ES */
            cpu_state.seg_es.seg    = 0;
            cpu_state.seg_es.base   = 0;
            cpu_state.seg_es.limit  = 0xFFFF;
            cpu_state.seg_es.access = data_attr & 0xFF;
            break;
        case 10: /* FS */
            cpu_state.seg_fs.seg    = 0;
            cpu_state.seg_fs.base   = 0;
            cpu_state.seg_fs.limit  = 0xFFFF;
            cpu_state.seg_fs.access = data_attr & 0xFF;
            break;
        case 11: /* GS */
            cpu_state.seg_gs.seg    = 0;
            cpu_state.seg_gs.base   = 0;
            cpu_state.seg_gs.limit  = 0xFFFF;
            cpu_state.seg_gs.access = data_attr & 0xFF;
            break;
        }
    }

    HRESULT hr = WHvSetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id, regs,
        sizeof(regs)/sizeof(regs[0]), vals);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvSetVirtualProcessorRegisters", hr);
        return -1;
    }
    pclog("whpx: VCPU registers initialized\n");
    return 0;
}

int whpx_init(void)
{
#if UINTPTR_MAX != 0xffffffffffffffffULL
    pclog("whpx: 32-bit build detected; WHPX requires a 64-bit executable\n");
#endif
    HRESULT hr;
#ifdef _WIN32
    /* WHvGetCapability expects a BOOL buffer. Using the 1-byte BOOLEAN
       type would cause WHV_E_INSUFFICIENT_BUFFER. */
    BOOL hypervisor_present = FALSE;

    /* Check that the Windows Hypervisor Platform service is available */
    UINT32 written = 0;
    hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
                          &hypervisor_present, sizeof(hypervisor_present),
                          &written);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvGetCapability(HypervisorPresent)", hr);
        return -1;
    }

    if (!hypervisor_present) {
        pclog("whpx: Hypervisor not present. Ensure virtualization is enabled "
              "and the 'Windows Hypervisor Platform' feature is installed.\n");
        return -1;
    }
#endif

    hr = WHvCreatePartition(&whpx_partition);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvCreatePartition", hr);
        return -1;
    }

    WHV_PARTITION_PROPERTY prop = {0};
    prop.ProcessorCount = 1;
    hr = WHvSetPartitionProperty(whpx_partition,
                                 WHvPartitionPropertyCodeProcessorCount,
                                 &prop, sizeof(prop));
    if (FAILED(hr)) {
        whpx_log_hresult("WHvSetPartitionProperty", hr);
        return -1;
    }

    hr = WHvSetupPartition(whpx_partition);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvSetupPartition", hr);
        return -1;
    }

    return 0;
}

void whpx_deinit(void)
{
    if (whpx_partition) {
        WHvDeletePartition(whpx_partition);
        whpx_partition = NULL;
        whpx_ram = NULL;
        whpx_ram_size = 0;
    }
}

int whpx_vcpu_create(void)
{
    if (!whpx_partition)
        return -1;
    if (whpx_vcpu_created)
        whpx_vcpu_destroy();
    pclog("Creating vCPU: partition=%p id=%u flags=0\n",
          whpx_partition, whpx_vcpu_id);
    HRESULT hr = WHvCreateVirtualProcessor(whpx_partition, whpx_vcpu_id, 0);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvCreateVirtualProcessor", hr);
#ifdef E_INVALIDARG
        if (hr == E_INVALIDARG)
            pclog("whpx: invalid arguments when creating the vCPU. "
                  "Ensure PCem is built for 64-bit and RAM is page aligned.\n");
#endif
        return -1;
    }
    whpx_vcpu_created = 1;
    pclog("whpx: Calling init_real_mode_registers()\n");
    if (init_real_mode_registers() != 0) {
        pclog("whpx: init_real_mode_registers failed\n");
        return -1;
    }
    whpx_dump_vp_registers("after init");
    return 0;
}

int whpx_map_memory(void *mem, size_t size)
{
    if (!whpx_partition)
        return -1;
    uintptr_t addr = (uintptr_t)mem;
    pclog("Mapping memory: addr=%p size=%zu (addr mod 4K=0x%lx size mod 4K=0x%lx)\n",
          mem, size, addr & 0xfff, (unsigned long)size & 0xfff);

    if (whpx_ram && whpx_ram_size) {
        pclog("Unmapping previous memory range size=%zu\n", whpx_ram_size);
        HRESULT hr2 = WHvUnmapGpaRange(whpx_partition, 0, whpx_ram_size);
        if (FAILED(hr2))
            whpx_log_hresult("WHvUnmapGpaRange", hr2);
    }

    whpx_ram = mem;
    whpx_ram_size = size;
    HRESULT hr = WHvMapGpaRange(whpx_partition, mem, 0, size,
                                 WHvMapGpaRangeFlagRead |
                                 WHvMapGpaRangeFlagWrite |
                                 WHvMapGpaRangeFlagExecute);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvMapGpaRange", hr);
#ifdef E_INVALIDARG
        if (hr == E_INVALIDARG)
            pclog("whpx: buffer or size not 4 KB aligned or running 32-bit build\n");
#endif
        return -1;
    }
    return 0;
}

int whpx_map_rom(const void *mem, unsigned long long gpa, size_t size)
{
    if (!whpx_partition)
        return -1;

    /* Replace any existing mapping for this GPA range */
    WHvUnmapGpaRange(whpx_partition, gpa, size);

    pclog("whpx: mapping ROM host=%p gpa=0x%llx size=0x%zx\n",
          mem, gpa, size);

    HRESULT hr = WHvMapGpaRange(whpx_partition, (void *)mem, gpa, size,
                                 WHvMapGpaRangeFlagRead |
                                 WHvMapGpaRangeFlagExecute);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvMapGpaRange(ROM)", hr);
        return -1;
    }
    return 0;
}

int whpx_map_range(void *mem, unsigned long long gpa, size_t size)
{
    if (!whpx_partition)
        return -1;

    HRESULT hr = WHvMapGpaRange(whpx_partition, mem, gpa, size,
                                 WHvMapGpaRangeFlagRead |
                                 WHvMapGpaRangeFlagWrite);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvMapGpaRange(range)", hr);
        return -1;
    }
    return 0;
}

int whpx_map_vga_memory(void *mem)
{
    return whpx_map_range(mem, 0xA0000, 0x20000);
}

void whpx_vcpu_destroy(void)
{
    if (whpx_partition && whpx_vcpu_created) {
        HRESULT hr = WHvDeleteVirtualProcessor(whpx_partition, whpx_vcpu_id);
        if (FAILED(hr))
            whpx_log_hresult("WHvDeleteVirtualProcessor", hr);
        whpx_vcpu_created = 0;
    }
}

static int whpx_sync_to_vcpu(void)
{
    WHV_REGISTER_NAME regs[16];
    WHV_REGISTER_VALUE vals[16];
    int idx = 0;

    regs[idx] = WHvX64RegisterRip;
    vals[idx++].Reg64 = cpu_state.pc;

    regs[idx] = WHvX64RegisterRax;
    vals[idx++].Reg64 = EAX;
    regs[idx] = WHvX64RegisterRbx;
    vals[idx++].Reg64 = EBX;
    regs[idx] = WHvX64RegisterRcx;
    vals[idx++].Reg64 = ECX;
    regs[idx] = WHvX64RegisterRdx;
    vals[idx++].Reg64 = EDX;
    regs[idx] = WHvX64RegisterRsi;
    vals[idx++].Reg64 = ESI;
    regs[idx] = WHvX64RegisterRdi;
    vals[idx++].Reg64 = EDI;
    regs[idx] = WHvX64RegisterRbp;
    vals[idx++].Reg64 = EBP;
    regs[idx] = WHvX64RegisterRsp;
    vals[idx++].Reg64 = ESP;

    regs[idx] = WHvX64RegisterRflags;
    /* Bit 1 of EFLAGS must always be set to 1 in x86. When the CPU is first
     * reset cpu_state.eflags may be 0, so ensure the reserved bit is set to
     * avoid WHPX rejecting the register state as invalid. */
    vals[idx++].Reg64 = (cpu_state.eflags & ~1ULL) | 0x2ULL;

    regs[idx] = WHvX64RegisterCs;
    vals[idx].Segment.Base = cs;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = CS;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_CODE_ATTR; /* present, execute/read */

    idx++;

    regs[idx] = WHvX64RegisterDs;
    vals[idx].Segment.Base = ds;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = DS;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_DATA_ATTR; /* present, read/write */
    idx++;

    regs[idx] = WHvX64RegisterEs;
    vals[idx].Segment.Base = es;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = ES;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_DATA_ATTR;
    idx++;

    regs[idx] = WHvX64RegisterSs;
    vals[idx].Segment.Base = ss;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = SS;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_DATA_ATTR;
    idx++;

    regs[idx] = WHvX64RegisterFs;
    vals[idx].Segment.Base = cpu_state.seg_fs.base;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = FS;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_DATA_ATTR;
    idx++;

    regs[idx] = WHvX64RegisterGs;
    vals[idx].Segment.Base = cpu_state.seg_gs.base;
    vals[idx].Segment.Limit = 0xFFFF;
    vals[idx].Segment.Selector = GS;
    SEGATTR(vals[idx].Segment) = WHPX_REAL_MODE_DATA_ATTR;
    idx++;

    HRESULT hr = WHvSetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id, regs, idx, vals);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvSetVirtualProcessorRegisters", hr);
        return -1;
    }
    return 0;
}

static int whpx_sync_from_vcpu(WHV_RUN_VP_EXIT_CONTEXT *ctx)
{
    WHV_REGISTER_NAME regs[16];
    WHV_REGISTER_VALUE vals[16];
    int idx = 0;

    regs[idx++] = WHvX64RegisterRip;
    regs[idx++] = WHvX64RegisterRax;
    regs[idx++] = WHvX64RegisterRbx;
    regs[idx++] = WHvX64RegisterRcx;
    regs[idx++] = WHvX64RegisterRdx;
    regs[idx++] = WHvX64RegisterRsi;
    regs[idx++] = WHvX64RegisterRdi;
    regs[idx++] = WHvX64RegisterRbp;
    regs[idx++] = WHvX64RegisterRsp;
    regs[idx++] = WHvX64RegisterRflags;
    regs[idx++] = WHvX64RegisterCs;
    regs[idx++] = WHvX64RegisterDs;
    regs[idx++] = WHvX64RegisterEs;
    regs[idx++] = WHvX64RegisterSs;
    regs[idx++] = WHvX64RegisterFs;
    regs[idx++] = WHvX64RegisterGs;

    HRESULT hr = WHvGetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id, regs, idx, vals);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvGetVirtualProcessorRegisters", hr);
        return -1;
    }

    idx = 0;
    cpu_state.pc = vals[idx++].Reg64;
    EAX = vals[idx++].Reg64;
    EBX = vals[idx++].Reg64;
    ECX = vals[idx++].Reg64;
    EDX = vals[idx++].Reg64;
    ESI = vals[idx++].Reg64;
    EDI = vals[idx++].Reg64;
    EBP = vals[idx++].Reg64;
    ESP = vals[idx++].Reg64;
    cpu_state.eflags = vals[idx++].Reg64;
    cpu_state.seg_cs.base = vals[idx].Segment.Base;
    cpu_state.seg_cs.limit = vals[idx].Segment.Limit;
    cpu_state.seg_cs.seg = vals[idx].Segment.Selector;
    cpu_state.seg_cs.access = SEGATTR(vals[idx].Segment);
    idx++;

    cpu_state.seg_ds.base = vals[idx].Segment.Base;
    cpu_state.seg_ds.limit = vals[idx].Segment.Limit;
    cpu_state.seg_ds.seg = vals[idx].Segment.Selector;
    cpu_state.seg_ds.access = SEGATTR(vals[idx].Segment);
    idx++;

    cpu_state.seg_es.base = vals[idx].Segment.Base;
    cpu_state.seg_es.limit = vals[idx].Segment.Limit;
    cpu_state.seg_es.seg = vals[idx].Segment.Selector;
    cpu_state.seg_es.access = SEGATTR(vals[idx].Segment);
    idx++;

    cpu_state.seg_ss.base = vals[idx].Segment.Base;
    cpu_state.seg_ss.limit = vals[idx].Segment.Limit;
    cpu_state.seg_ss.seg = vals[idx].Segment.Selector;
    cpu_state.seg_ss.access = SEGATTR(vals[idx].Segment);
    idx++;

    cpu_state.seg_fs.base = vals[idx].Segment.Base;
    cpu_state.seg_fs.limit = vals[idx].Segment.Limit;
    cpu_state.seg_fs.seg = vals[idx].Segment.Selector;
    cpu_state.seg_fs.access = SEGATTR(vals[idx].Segment);
    idx++;

    cpu_state.seg_gs.base = vals[idx].Segment.Base;
    cpu_state.seg_gs.limit = vals[idx].Segment.Limit;
    cpu_state.seg_gs.seg = vals[idx].Segment.Selector;
    cpu_state.seg_gs.access = SEGATTR(vals[idx].Segment);
    idx++;

    return 0;
}

/* Ensure all segment descriptors remain marked present */
static void ensure_valid_segments(void)
{
    WHV_REGISTER_NAME segs[] = {
        WHvX64RegisterCs, WHvX64RegisterSs, WHvX64RegisterDs,
        WHvX64RegisterEs, WHvX64RegisterFs, WHvX64RegisterGs};
    WHV_REGISTER_VALUE vals[6];

    HRESULT hr = WHvGetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id, segs, 6, vals);
    if (FAILED(hr)) {
        whpx_log_hresult("WHvGetVirtualProcessorRegisters", hr);
        return;
    }

    bool changed = false;
    for (int i = 0; i < 6; ++i) {
        if (!(SEGATTR(vals[i].Segment) & 0x0080)) {
            vals[i].Segment.Limit = 0xFFFF;
            vals[i].Segment.Base = vals[i].Segment.Selector << 4;
            SEGATTR(vals[i].Segment) |=
                (i == 0 ? WHPX_REAL_MODE_CODE_ATTR : WHPX_REAL_MODE_DATA_ATTR) &
                0x00F7;
            changed = true;
        }
    }

    if (changed) {
        hr = WHvSetVirtualProcessorRegisters(
            whpx_partition, whpx_vcpu_id, segs, 6, vals);
        if (FAILED(hr))
            whpx_log_hresult("WHvSetVirtualProcessorRegisters", hr);
    }
}

int whpx_vcpu_run(void)
{
    WHV_RUN_VP_EXIT_CONTEXT exit_ctx = {0};
    if (!whpx_partition)
        return -1;

    if (whpx_sync_to_vcpu() != 0)
        return -1;

    /* Validate that all segment descriptors remain present */
    ensure_valid_segments();

    if (whpx_first_run) {
        whpx_dump_vp_registers("before first run");
        whpx_first_run = 0;
    }

    HRESULT hr = WHvRunVirtualProcessor(whpx_partition, whpx_vcpu_id, &exit_ctx,
                                         sizeof(exit_ctx));
    if (FAILED(hr)) {
        whpx_log_hresult("WHvRunVirtualProcessor", hr);
        return -1;
    }

    if (whpx_sync_from_vcpu(&exit_ctx) != 0)
        return -1;

    pclog("whpx: exit reason %u\n", exit_ctx.ExitReason);

    /* Some common WHPX exit reasons for reference:
       5 = WHvRunVpExitReasonInvalidVpRegisterValue
       6 = WHvRunVpExitReasonUnsupportedFeature
       7 = WHvRunVpExitReasonX64InterruptWindow
       8 = WHvRunVpExitReasonX64Halt
    */

    switch (exit_ctx.ExitReason) {
    case WHvRunVpExitReasonInvalidVpRegisterValue:
        pclog("whpx: \xE2\x9D\x8C Invalid VP register value (exit reason 5)\n");
        /* Dump registers to help diagnose misconfigured segments */
        whpx_dump_vp_registers("invalid register value");
        return -1;
    case WHvRunVpExitReasonX64Halt:
        pclog("whpx: HLT encountered, continuing execution\n");
        if (cpu_state.pc == 0xFFF0) {
            const uint8_t *p = rom + 0x3FFF0;
            pclog("whpx: BIOS vector bytes: %02X %02X %02X %02X\n",
                  p[0], p[1], p[2], p[3]);
        }
        /* continue execution so the interpreter can handle pending IRQs */
        return 0;
    case WHvRunVpExitReasonMemoryAccess:
    case WHvRunVpExitReasonX64IoPortAccess:
        /* Unhandled exits will be emulated by the interpreter */
        return 0;
#ifdef WHvRunVpExitReasonNone
    case WHvRunVpExitReasonNone:
        pclog("whpx: no exit reason provided; vCPU state unchanged\n");
        return -1;
#endif
    default:
        pclog("whpx: unexpected exit reason %u\n", exit_ctx.ExitReason);
        /* Handle unrecognized exits with the interpreter */
        return 0;
    }
}

#else /* !_WIN32 */
int whpx_init(void) { return -1; }
void whpx_deinit(void) {}
int whpx_vcpu_create(void) { return -1; }
void whpx_vcpu_destroy(void) {}
int whpx_vcpu_run(void) { return -1; }
int whpx_map_memory(void *mem, size_t size) { return -1; }
int whpx_map_range(void *mem, unsigned long long gpa, size_t size) { return -1; }
int whpx_map_vga_memory(void *mem) { return -1; }
#endif /* _WIN32 */

#else /* !USE_WHPX */
int whpx_dummy; /* avoid empty object file */
#endif /* USE_WHPX */
