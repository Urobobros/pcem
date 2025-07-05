#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#endif

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
        fprintf(stderr, "WHvGetCapability failed: 0x%lx\n", hr);
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
        fprintf(stderr, "WHvCreatePartition failed: 0x%lx\n", hr);
        return 1;
    }

    WHV_PARTITION_PROPERTY prop = {0};
    prop.ProcessorCount = 1;
    hr = WHvSetPartitionProperty(partition,
                                 WHvPartitionPropertyCodeProcessorCount,
                                 &prop, sizeof(prop));
    if (FAILED(hr)) {
        fprintf(stderr, "WHvSetPartitionProperty failed: 0x%lx\n", hr);
        WHvDeletePartition(partition);
        return 1;
    }

    hr = WHvSetupPartition(partition);
    if (FAILED(hr)) {
        fprintf(stderr, "WHvSetupPartition failed: 0x%lx\n", hr);
        WHvDeletePartition(partition);
        return 1;
    }

    /* Allocate one page with a HLT instruction */
    void *ram = VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
        fprintf(stderr, "WHvMapGpaRange failed: 0x%lx\n", hr);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    hr = WHvCreateVirtualProcessor(partition, 0, 0);
    if (FAILED(hr)) {
        fprintf(stderr, "WHvCreateVirtualProcessor failed: 0x%lx\n", hr);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    WHV_REGISTER_NAME reg_name = WHvX64RegisterRip;
    WHV_REGISTER_VALUE reg_val = {0};
    hr = WHvSetVirtualProcessorRegisters(partition, 0,
                                         &reg_name, 1, &reg_val);
    if (FAILED(hr)) {
        fprintf(stderr, "WHvSetVirtualProcessorRegisters failed: 0x%lx\n", hr);
        WHvDeleteVirtualProcessor(partition, 0);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    WHV_RUN_VP_EXIT_CONTEXT exit_ctx;
    hr = WHvRunVirtualProcessor(partition, 0, &exit_ctx, sizeof(exit_ctx));
    if (FAILED(hr)) {
        fprintf(stderr, "WHvRunVirtualProcessor failed: 0x%lx\n", hr);
    } else if (exit_ctx.ExitReason == WHvRunVpExitReasonX64Halt) {
        printf("Windows Hypervisor Platform is available and functional.\n");
    } else {
        printf("Unexpected exit reason %u\n", exit_ctx.ExitReason);
    }

    WHvDeleteVirtualProcessor(partition, 0);
    VirtualFree(ram, 0, MEM_RELEASE);
    WHvDeletePartition(partition);
    return 0;
#else
    printf("This tool only runs on Windows.\n");
    return 0;
#endif
}
