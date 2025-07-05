#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#endif

int main(void)
{
#ifdef _WIN32
    HRESULT hr;
    BOOLEAN hypervisor_present = FALSE;
    hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
                          &hypervisor_present,
                          sizeof(hypervisor_present),
                          NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "WHvGetCapability failed: 0x%lx\n", hr);
        return 1;
    }
    if (!hypervisor_present) {
        printf("Hypervisor not present. Make sure virtualization is enabled and the Windows Hypervisor Platform feature is installed.\n");
        return 2;
    }
    WHV_PARTITION_HANDLE partition = NULL;
    hr = WHvCreatePartition(&partition);
    if (FAILED(hr)) {
        fprintf(stderr, "WHvCreatePartition failed: 0x%lx\n", hr);
        return 1;
    }
    WHvDeletePartition(partition);
    printf("Windows Hypervisor Platform is available and functional.\n");
    return 0;
#else
    printf("This tool only runs on Windows.\n");
    return 0;
#endif
}
