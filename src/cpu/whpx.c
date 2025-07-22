#ifdef _WIN32
#include <Windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#endif
#include "ibm.h"
#include "cpu.h"
#include "mem.h"

#ifdef _WIN32
static WHV_EMULATOR_HANDLE whpx_emulator;
static WHV_PARTITION_HANDLE whpx_partition;
static void *whpx_mem;
static UINT64 whpx_mem_size;

static HRESULT CALLBACK whpx_io_cb(void *ctx, WHV_EMULATOR_IO_ACCESS_INFO *io)
{
    (void)ctx;
    if (io->Direction)
    {
        switch (io->AccessSize)
        {
        case 1: outb((uint16_t)io->Port, (uint8_t)io->Data); break;
        case 2: outw((uint16_t)io->Port, (uint16_t)io->Data); break;
        case 4: outl((uint16_t)io->Port, (uint32_t)io->Data); break;
        }
    }
    else
    {
        uint32_t val = 0;
        switch (io->AccessSize)
        {
        case 1: val = inb((uint16_t)io->Port); break;
        case 2: val = inw((uint16_t)io->Port); break;
        case 4: val = inl((uint16_t)io->Port); break;
        }
        io->Data = val;
    }
    return S_OK;
}

static HRESULT CALLBACK whpx_mmio_cb(void *ctx, WHV_EMULATOR_MEMORY_ACCESS_INFO *mem)
{
    (void)ctx;
    UINT64 addr = mem->GpaAddress;
    for (UINT32 i = 0; i < mem->AccessSize; i++) {
        if (mem->Direction)
            mem_writeb_phys((uint32_t)addr + i, ((uint8_t *)mem->Data)[i]);
        else
            ((uint8_t *)mem->Data)[i] = mem_readb_phys((uint32_t)addr + i);
    }
    return S_OK;
}

static HRESULT CALLBACK whpx_get_regs_cb(void *ctx, const WHV_REGISTER_NAME *names,
                                         UINT32 count, WHV_REGISTER_VALUE *values)
{
    (void)ctx;
    return WHvGetVirtualProcessorRegisters(whpx_partition, 0, names, count, values);
}

static HRESULT CALLBACK whpx_set_regs_cb(void *ctx, const WHV_REGISTER_NAME *names,
                                         UINT32 count, const WHV_REGISTER_VALUE *values)
{
    (void)ctx;
    return WHvSetVirtualProcessorRegisters(whpx_partition, 0, names, count, values);
}

static HRESULT CALLBACK whpx_translate_cb(void *ctx, WHV_GUEST_VIRTUAL_ADDRESS gva,
                                          WHV_TRANSLATE_GVA_FLAGS flags,
                                          WHV_TRANSLATE_GVA_RESULT_CODE *result,
                                          WHV_GUEST_PHYSICAL_ADDRESS *gpa)
{
    (void)ctx;
    WHV_TRANSLATE_GVA_RESULT r;
    HRESULT hr = WHvTranslateGva(whpx_partition, 0, gva, flags, &r, gpa);
    *result = r.ResultCode;
    return hr;
}
#endif /* _WIN32 */

void whpx_init(void)
{
#ifdef _WIN32
    WHV_CAPABILITY cap = {0};
    UINT32 ret_len = 0;
    HRESULT hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &cap,
                                   sizeof(cap), &ret_len);
    if (FAILED(hr) || !cap.HypervisorPresent) {
        pclog("whpx: hypervisor not present\n");
        return;
    }

    WHV_EMULATOR_CALLBACKS cbs = {0};
    cbs.Size = sizeof(cbs);
    cbs.WHvEmulatorIoPortCallback = whpx_io_cb;
    cbs.WHvEmulatorMemoryCallback = whpx_mmio_cb;
    cbs.WHvEmulatorGetVirtualProcessorRegisters = whpx_get_regs_cb;
    cbs.WHvEmulatorSetVirtualProcessorRegisters = whpx_set_regs_cb;
    cbs.WHvEmulatorTranslateGvaPage = whpx_translate_cb;

    hr = WHvEmulatorCreateEmulator(&cbs, &whpx_emulator);
    if (FAILED(hr)) {
        pclog("whpx: emulator create failed %lx\n", (unsigned long)hr);
        return;
    }

    hr = WHvCreatePartition(&whpx_partition);
    if (FAILED(hr)) {
        pclog("whpx: create partition failed %lx\n", (unsigned long)hr);
        WHvEmulatorDestroyEmulator(whpx_emulator);
        whpx_emulator = NULL;
        return;
    }

    UINT32 count = 1;
    WHvSetPartitionProperty(whpx_partition, WHvPartitionPropertyCodeProcessorCount,
                            &count, sizeof(count));
    WHvSetupPartition(whpx_partition);

    whpx_mem_size = (UINT64)mem_size * 1024 + 0x100000;
    whpx_mem = VirtualAlloc(NULL, (SIZE_T)whpx_mem_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!whpx_mem) {
        pclog("whpx: memory alloc failed\n");
        WHvDeletePartition(whpx_partition);
        WHvEmulatorDestroyEmulator(whpx_emulator);
        whpx_partition = NULL;
        whpx_emulator = NULL;
        return;
    }

    WHvMapGpaRange(whpx_partition, whpx_mem, 0, whpx_mem_size,
                   WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);

    WHvCreateVirtualProcessor(whpx_partition, 0, 0);

    WHV_REGISTER_NAME names[] = {WHvX64RegisterRip, WHvX64RegisterRflags,
                                WHvX64RegisterCs, WHvX64RegisterRax};
    WHV_REGISTER_VALUE values[4] = {0};
    values[0].Reg64 = 0xffff0; /* reset vector */
    values[1].Reg64 = 0x2;     /* default flags */
    values[2].Segment.Selector = 0xf000;
    values[2].Segment.Base = 0xf0000;
    values[2].Segment.Limit = 0xffff;
    values[2].Segment.Attributes = 0x93;
    values[3].Reg64 = 0;
    WHvSetVirtualProcessorRegisters(whpx_partition, 0, names, 4, values);

    pclog("whpx: initialized\n");
#else
    pclog("whpx: not supported\n");
#endif
}

void whpx_run(int ncycles)
{
#ifdef _WIN32
    WHV_RUN_VP_EXIT_CONTEXT exitctx;
    for (int i = 0; i < ncycles; ) {
        HRESULT hr = WHvRunVirtualProcessor(whpx_partition, 0, &exitctx, sizeof(exitctx));
        if (FAILED(hr)) break;
        switch (exitctx.ExitReason) {
        case WHvRunVpExitReasonX64IoPortAccess:
            WHvEmulatorTryIoEmulation(whpx_emulator, NULL, &exitctx.VpContext,
                                      &exitctx.IoPortAccess, NULL);
            break;
        case WHvRunVpExitReasonMemoryAccess:
            WHvEmulatorTryMmioEmulation(whpx_emulator, NULL, &exitctx.VpContext,
                                        &exitctx.MemoryAccess, NULL);
            break;
        case WHvRunVpExitReasonX64Halt:
            i = ncycles; /* exit */
            break;
        default:
            i = ncycles;
            break;
        }
        i++;
    }
#else
    (void)ncycles;
#endif
}

void whpx_shutdown(void)
{
#ifdef _WIN32
    if (whpx_partition) {
        WHvDeleteVirtualProcessor(whpx_partition, 0);
        WHvDeletePartition(whpx_partition);
        whpx_partition = NULL;
    }
    if (whpx_mem) {
        VirtualFree(whpx_mem, 0, MEM_RELEASE);
        whpx_mem = NULL;
    }
    if (whpx_emulator) {
        WHvEmulatorDestroyEmulator(whpx_emulator);
        whpx_emulator = NULL;
    }
#endif
}
