#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
/* MinGW headers expose segment attributes in the Flags field */
#define SEGATTR(seg) ((seg).Flags)
#else
/* Windows SDK headers wrap the attributes inside a union */
#define SEGATTR(seg) ((seg).Attributes.AsUINT16)
#endif
#endif

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

static void log_hresult(const char *prefix, HRESULT hr)
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
            fprintf(stderr, "%s failed: 0x%lx (%s) - %s\n", prefix, hr, name, msg);
        else
            fprintf(stderr, "%s failed: 0x%lx - %s\n", prefix, hr, msg);
        LocalFree(msg);
    } else {
        if (name)
            fprintf(stderr, "%s failed: 0x%lx (%s)\n", prefix, hr, name);
        else
            fprintf(stderr, "%s failed: 0x%lx\n", prefix, hr);
    }
}

int main(void)
{
#ifdef _WIN32
    HRESULT hr;
    /* BOOL is 4 bytes while BOOLEAN is 1 byte. WHvGetCapability expects
       a 4-byte BOOL buffer so using BOOLEAN would trigger
       WHV_E_INSUFFICIENT_BUFFER (0x80370301). */
    BOOL hypervisor_present = FALSE;
    UINT32 written = 0;
    hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
                          &hypervisor_present,
                          sizeof(hypervisor_present),
                          &written);
    if (FAILED(hr)) {
        log_hresult("WHvGetCapability", hr);
#ifdef WHV_E_UNKNOWN_CAPABILITY
        if (hr == WHV_E_UNKNOWN_CAPABILITY)
            fprintf(stderr, "The installed Windows version does not support WHPX or the feature is missing.\n");
#endif
        return 1;
    }
    if (!hypervisor_present) {
        printf("WHPX NO - Hypervisor not present. Make sure virtualization is enabled and the Windows Hypervisor Platform feature is installed.\n");
        return 2;
    }
    WHV_PARTITION_HANDLE partition = NULL;
    hr = WHvCreatePartition(&partition);
    if (FAILED(hr)) {
        log_hresult("WHvCreatePartition", hr);
        return 1;
    }

    WHV_PARTITION_PROPERTY prop = {0};
    prop.ProcessorCount = 1;
    hr = WHvSetPartitionProperty(partition,
                                 WHvPartitionPropertyCodeProcessorCount,
                                 &prop, sizeof(prop));
    if (FAILED(hr)) {
        log_hresult("WHvSetPartitionProperty", hr);
        WHvDeletePartition(partition);
        return 1;
    }

    hr = WHvSetupPartition(partition);
    if (FAILED(hr)) {
        log_hresult("WHvSetupPartition", hr);
        WHvDeletePartition(partition);
        return 1;
    }

    /* Allocate one page with a HLT instruction */
    void *ram = VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
    if (!ram) {
        fprintf(stderr, "VirtualAlloc failed\n");
        WHvDeletePartition(partition);
        return 1;
    }
    ((unsigned char *)ram)[0] = 0xF4; /* HLT */

    hr = WHvMapGpaRange(partition, ram, 0, 0x1000,
                        WHvMapGpaRangeFlagRead |
                        WHvMapGpaRangeFlagWrite |
                        WHvMapGpaRangeFlagExecute);
    if (FAILED(hr)) {
        log_hresult("WHvMapGpaRange", hr);
#ifdef E_INVALIDARG
        if (hr == E_INVALIDARG)
            fprintf(stderr, "Invalid mapping parameters. Make sure this"
                            " program is built as 64-bit and uses 4 KB"
                            " aligned memory.\n");
#endif
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    /* Map VGA memory range at 0xA0000 - 0xC0000 */
    void *vga = VirtualAlloc(NULL, 0x20000, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
    if (vga) {
        hr = WHvMapGpaRange(partition, vga, 0xA0000, 0x20000,
                            WHvMapGpaRangeFlagRead |
                            WHvMapGpaRangeFlagWrite);
        if (FAILED(hr)) {
            log_hresult("WHvMapGpaRange (VGA)", hr);
        }
    }

    hr = WHvCreateVirtualProcessor(partition, 0, 0);
    if (FAILED(hr)) {
        log_hresult("WHvCreateVirtualProcessor", hr);
        VirtualFree(ram, 0, MEM_RELEASE);
        VirtualFree(vga, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    /*
     * Initialize a minimal CPU state so WHPX can start execution. In
     * addition to RIP we must provide a valid code segment and stack
     * pointer. Without these the hypervisor stops the vCPU with a
     * MemoryAccess exit before our HLT executes.
     */
    WHV_REGISTER_NAME reg_names[] = {
        WHvX64RegisterRip,
        WHvX64RegisterCs,
        WHvX64RegisterRsp
    };
    WHV_REGISTER_VALUE reg_vals[3] = {0};

    /* RIP starts at GPA 0 where we placed the HLT instruction. */
    reg_vals[0].Reg64 = 0;

    /* Minimal flat code segment descriptor. */
    reg_vals[1].Segment.Base = 0;
    reg_vals[1].Segment.Limit = 0xFFFFFFFF;
    reg_vals[1].Segment.Selector = 0;
    SEGATTR(reg_vals[1].Segment) = 0xC09B; /* 32-bit code, present, ring 0 */

    /* Provide a stack pointer within the mapped page. */
    reg_vals[2].Reg64 = 0x800;

    hr = WHvSetVirtualProcessorRegisters(partition, 0,
                                         reg_names, 3, reg_vals);
    if (FAILED(hr)) {
        log_hresult("WHvSetVirtualProcessorRegisters", hr);
        WHvDeleteVirtualProcessor(partition, 0);
        VirtualFree(ram, 0, MEM_RELEASE);
        VirtualFree(vga, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    WHV_RUN_VP_EXIT_CONTEXT exit_ctx;
    hr = WHvRunVirtualProcessor(partition, 0, &exit_ctx, sizeof(exit_ctx));
    if (FAILED(hr)) {
        log_hresult("WHvRunVirtualProcessor", hr);
    } else if (exit_ctx.ExitReason == WHvRunVpExitReasonX64Halt) {
        printf("Windows Hypervisor Platform is available and functional.\n");
    } else {
        printf("Unexpected exit reason %u\n", exit_ctx.ExitReason);
    }

    /* Dump registers for diagnostic purposes */
    WHV_REGISTER_NAME dump_regs[] = {
        WHvX64RegisterRip,
        WHvX64RegisterCs,
        WHvX64RegisterSs,
        WHvX64RegisterDs,
        WHvX64RegisterEs,
        WHvX64RegisterFs,
        WHvX64RegisterGs,
        WHvX64RegisterRsp,
        WHvX64RegisterCr0,
        WHvX64RegisterCr3,
        WHvX64RegisterCr4,
        WHvX64RegisterRflags,
        WHvX64RegisterEfer
    };
    WHV_REGISTER_VALUE reg_out[sizeof(dump_regs) / sizeof(dump_regs[0])] = {0};
    hr = WHvGetVirtualProcessorRegisters(partition, 0, dump_regs,
                                         sizeof(dump_regs) / sizeof(dump_regs[0]),
                                         reg_out);
    if (SUCCEEDED(hr)) {
        printf("RIP=0x%llX  RSP=0x%llX  FLAGS=0x%llX  CS=0x%04X base=0x%llX attr=0x%04X\n",
               reg_out[0].Reg64,
               reg_out[7].Reg64,
               reg_out[11].Reg64,
               reg_out[1].Segment.Selector,
               reg_out[1].Segment.Base,
               SEGATTR(reg_out[1].Segment));
        printf("CR0=0x%llX CR3=0x%llX CR4=0x%llX EFER=0x%llX\n",
               reg_out[8].Reg64,
               reg_out[9].Reg64,
               reg_out[10].Reg64,
               reg_out[12].Reg64);
    }

    WHvDeleteVirtualProcessor(partition, 0);
    VirtualFree(ram, 0, MEM_RELEASE);
    VirtualFree(vga, 0, MEM_RELEASE);
    WHvDeletePartition(partition);
    return 0;
#else
    printf("This tool only runs on Windows.\n");
    return 0;
#endif
}
