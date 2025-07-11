#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>

#ifdef __MINGW32__
#define SEGATTR(seg) ((seg).Attributes)
#elif defined(_MSC_VER)
#define SEGATTR(seg) ((seg).Attributes.AsUINT16)
#else
#define SEGATTR(seg) ((seg).Flags)
#endif

static void log_hresult(const char *prefix, HRESULT hr)
{
    char *msg = NULL;
    HMODULE mod = GetModuleHandleA("WinHvPlatform.dll");
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_FROM_HMODULE |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   mod, hr, 0, (LPSTR)&msg, 0, NULL);
    if (msg) {
        size_t len = strlen(msg);
        while (len && (msg[len - 1] == '\n' || msg[len - 1] == '\r'))
            msg[--len] = '\0';
        fprintf(stderr, "%s failed: 0x%lx - %s\n", prefix, hr, msg);
        LocalFree(msg);
    } else {
        fprintf(stderr, "%s failed: 0x%lx\n", prefix, hr);
    }
}
#endif /* _WIN32 */

int main(int argc, char **argv)
{
#ifdef _WIN32
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bios.rom>\n", argv[0]);
        return 1;
    }
    const char *rom_path = argv[1];
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    /*
     * BIOS images for PCem boards are often larger than 64 kB.  The
     * original loader only accepted up to 0x10000 bytes which prevented
     * testing of 128 kB and 256 kB ROMs (for example the 430VX BIOS).
     * Allow images up to 256 kB which covers all current PCem ROMs.
     */
    if (rom_size <= 0 || rom_size > 0x40000) {
        fprintf(stderr, "unsupported ROM size\n");
        fclose(f);
        return 1;
    }

    void *rom = malloc(rom_size);
    if (!rom) {
        fprintf(stderr, "malloc failed\n");
        fclose(f);
        return 1;
    }
    if (fread(rom, 1, rom_size, f) != (size_t)rom_size) {
        fprintf(stderr, "read failed\n");
        free(rom);
        fclose(f);
        return 1;
    }
    fclose(f);

    HRESULT hr;
    BOOL hypervisor_present = FALSE;
    UINT32 written = 0;
    hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
                          &hypervisor_present,
                          sizeof(hypervisor_present),
                          &written);
    if (FAILED(hr)) {
        log_hresult("WHvGetCapability", hr);
        free(rom);
        return 1;
    }
    if (!hypervisor_present) {
        fprintf(stderr, "Hypervisor not present\n");
        free(rom);
        return 1;
    }

    WHV_PARTITION_HANDLE partition = NULL;
    hr = WHvCreatePartition(&partition);
    if (FAILED(hr)) {
        log_hresult("WHvCreatePartition", hr);
        free(rom);
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
        free(rom);
        return 1;
    }

    hr = WHvSetupPartition(partition);
    if (FAILED(hr)) {
        log_hresult("WHvSetupPartition", hr);
        WHvDeletePartition(partition);
        free(rom);
        return 1;
    }

    void *ram = VirtualAlloc(NULL, 0x100000, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
    if (!ram) {
        fprintf(stderr, "VirtualAlloc failed\n");
        WHvDeletePartition(partition);
        free(rom);
        return 1;
    }

    memcpy((unsigned char *)ram + 0x100000 - rom_size, rom, rom_size);
    free(rom);

    hr = WHvMapGpaRange(partition, ram, 0, 0x100000,
                        WHvMapGpaRangeFlagRead |
                        WHvMapGpaRangeFlagWrite |
                        WHvMapGpaRangeFlagExecute);
    if (FAILED(hr)) {
        log_hresult("WHvMapGpaRange", hr);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    hr = WHvCreateVirtualProcessor(partition, 0, 0);
    if (FAILED(hr)) {
        log_hresult("WHvCreateVirtualProcessor", hr);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    WHV_REGISTER_NAME regs[10];
    WHV_REGISTER_VALUE vals[10];
    int n = 0;

    regs[n] = WHvX64RegisterRip;
    vals[n].Reg64 = 0xFFF0;
    n++;

    regs[n] = WHvX64RegisterCs;
    vals[n].Segment.Base = 0xF0000;
    vals[n].Segment.Limit = 0xFFFF;
    vals[n].Segment.Selector = 0xF000;
    SEGATTR(vals[n].Segment) = 0x0093;
    n++;

    regs[n] = WHvX64RegisterRsp;
    vals[n].Reg64 = 0x8000;
    n++;

    WHV_REGISTER_NAME segs[] = { WHvX64RegisterDs, WHvX64RegisterEs,
                                 WHvX64RegisterSs, WHvX64RegisterFs,
                                 WHvX64RegisterGs };
    for (int i = 0; i < 5; i++) {
        regs[n] = segs[i];
        vals[n].Segment.Base = 0;
        vals[n].Segment.Limit = 0xFFFF;
        vals[n].Segment.Selector = 0;
        SEGATTR(vals[n].Segment) = 0x0092;
        n++;
    }

    regs[n] = WHvX64RegisterCr0;
    vals[n].Reg64 = 0x10;
    n++;

    regs[n] = WHvX64RegisterRflags;
    vals[n].Reg64 = 0x2;
    n++;

    hr = WHvSetVirtualProcessorRegisters(partition, 0, regs, n, vals);
    if (FAILED(hr)) {
        log_hresult("WHvSetVirtualProcessorRegisters", hr);
        WHvDeleteVirtualProcessor(partition, 0);
        VirtualFree(ram, 0, MEM_RELEASE);
        WHvDeletePartition(partition);
        return 1;
    }

    WHV_RUN_VP_EXIT_CONTEXT exit_ctx;
    hr = WHvRunVirtualProcessor(partition, 0, &exit_ctx, sizeof(exit_ctx));
    if (FAILED(hr)) {
        log_hresult("WHvRunVirtualProcessor", hr);
    } else {
        printf("Exit reason %u at RIP=0x%llX\n", exit_ctx.ExitReason, exit_ctx.VpContext.Rip);
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
