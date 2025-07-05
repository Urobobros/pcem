#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
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
    WHvDeletePartition(partition);
    printf("Windows Hypervisor Platform is available and functional.\n");
    return 0;
#else
    printf("This tool only runs on Windows.\n");
    return 0;
#endif
}
