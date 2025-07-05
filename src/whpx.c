#ifdef USE_WHPX
#include "whpx.h"
#include "x86.h"
#include "ibm.h"

#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>

#define SEGATTR(seg) ((seg).Attributes.AsUINT16)

static WHV_PARTITION_HANDLE whpx_partition = NULL;
static UINT32 whpx_vcpu_id = 0;
static void *whpx_ram = NULL;
static size_t whpx_ram_size = 0;

int whpx_init(void)
{
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
        pclog("whpx: WHvGetCapability(HypervisorPresent) failed: 0x%lx\n", hr);
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
        pclog("whpx: WHvCreatePartition failed: 0x%lx\n", hr);
        return -1;
    }

    WHV_PARTITION_PROPERTY prop = {0};
    prop.ProcessorCount = 1;
    hr = WHvSetPartitionProperty(whpx_partition,
                                 WHvPartitionPropertyCodeProcessorCount,
                                 &prop, sizeof(prop));
    if (FAILED(hr)) {
        pclog("whpx: WHvSetPartitionProperty failed: 0x%lx\n", hr);
        return -1;
    }

    hr = WHvSetupPartition(whpx_partition);
    if (FAILED(hr)) {
        pclog("whpx: WHvSetupPartition failed: 0x%lx\n", hr);
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
    HRESULT hr = WHvCreateVirtualProcessor(whpx_partition, whpx_vcpu_id, 0);
    if (FAILED(hr)) {
        pclog("whpx: WHvCreateVirtualProcessor failed: 0x%lx\n", hr);
        return -1;
    }
    return 0;
}

int whpx_map_memory(void *mem, size_t size)
{
    if (!whpx_partition)
        return -1;
    whpx_ram = mem;
    whpx_ram_size = size;
    HRESULT hr = WHvMapGpaRange(whpx_partition, mem, 0, size,
                                 WHvMapGpaRangeFlagRead |
                                 WHvMapGpaRangeFlagWrite |
                                 WHvMapGpaRangeFlagExecute);
    if (FAILED(hr)) {
        pclog("whpx: WHvMapGpaRange failed: 0x%lx\n", hr);
#ifdef E_INVALIDARG
        if (hr == E_INVALIDARG)
            pclog("whpx: buffer or size not 4 KB aligned or running 32-bit build\n");
#endif
        return -1;
    }
    return 0;
}

void whpx_vcpu_destroy(void)
{
    if (whpx_partition) {
        HRESULT hr = WHvDeleteVirtualProcessor(whpx_partition, whpx_vcpu_id);
        if (FAILED(hr))
            pclog("whpx: WHvDeleteVirtualProcessor failed: 0x%lx\n", hr);
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
    vals[idx++].Reg64 = cpu_state.eflags;

    regs[idx] = WHvX64RegisterCs;
    vals[idx].Segment.Base = cs;
    vals[idx].Segment.Limit = cpu_state.seg_cs.limit;
    vals[idx].Segment.Selector = CS;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_cs.access;
    idx++;

    regs[idx] = WHvX64RegisterDs;
    vals[idx].Segment.Base = ds;
    vals[idx].Segment.Limit = cpu_state.seg_ds.limit;
    vals[idx].Segment.Selector = DS;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_ds.access;
    idx++;

    regs[idx] = WHvX64RegisterEs;
    vals[idx].Segment.Base = es;
    vals[idx].Segment.Limit = cpu_state.seg_es.limit;
    vals[idx].Segment.Selector = ES;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_es.access;
    idx++;

    regs[idx] = WHvX64RegisterSs;
    vals[idx].Segment.Base = ss;
    vals[idx].Segment.Limit = cpu_state.seg_ss.limit;
    vals[idx].Segment.Selector = SS;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_ss.access;
    idx++;

    regs[idx] = WHvX64RegisterFs;
    vals[idx].Segment.Base = cpu_state.seg_fs.base;
    vals[idx].Segment.Limit = cpu_state.seg_fs.limit;
    vals[idx].Segment.Selector = FS;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_fs.access;
    idx++;

    regs[idx] = WHvX64RegisterGs;
    vals[idx].Segment.Base = cpu_state.seg_gs.base;
    vals[idx].Segment.Limit = cpu_state.seg_gs.limit;
    vals[idx].Segment.Selector = GS;
    SEGATTR(vals[idx].Segment) = cpu_state.seg_gs.access;
    idx++;

    HRESULT hr = WHvSetVirtualProcessorRegisters(
        whpx_partition, whpx_vcpu_id, regs, idx, vals);
    if (FAILED(hr)) {
        pclog("whpx: WHvSetVirtualProcessorRegisters failed: 0x%lx\n", hr);
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
        pclog("whpx: WHvGetVirtualProcessorRegisters failed: 0x%lx\n", hr);
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

int whpx_vcpu_run(void)
{
    WHV_RUN_VP_EXIT_CONTEXT exit_ctx;
    if (!whpx_partition)
        return -1;

    if (whpx_sync_to_vcpu() != 0)
        return -1;

    HRESULT hr = WHvRunVirtualProcessor(whpx_partition, whpx_vcpu_id, &exit_ctx,
                                         sizeof(exit_ctx));
    if (FAILED(hr)) {
        pclog("whpx: WHvRunVirtualProcessor failed: 0x%lx\n", hr);
        return -1;
    }

    if (whpx_sync_from_vcpu(&exit_ctx) != 0)
        return -1;

    switch (exit_ctx.ExitReason) {
    case WHvRunVpExitReasonX64Halt:
        return 1;
    case WHvRunVpExitReasonMemoryAccess:
    case WHvRunVpExitReasonX64IoPortAccess:
        /* Unhandled exits will be emulated by the interpreter */
        return 0;
    default:
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
#endif /* _WIN32 */

#else /* !USE_WHPX */
int whpx_dummy; /* avoid empty object file */
#endif /* USE_WHPX */
