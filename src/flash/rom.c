#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "ibm.h"
#include "config.h"
#include "mem.h"
#include "rom.h"
#include "paths.h"
#if defined(_WIN32) && defined(USE_WHPX)
#include <windows.h>
#include "cpu_backend.h"
#include "whpx.h"
#endif

FILE *romfopen(char *fn, char *mode) {
        FILE *f;
        char s[512];
        int i;

        for (i = 0; i < num_roms_paths; ++i) {
                get_roms_path(i, s, 511);
                put_backslash(s);
                strcat(s, fn);
                f = fopen(s, mode);
                if (f)
                        return f;
        }
        return 0;
}

int rom_present(char *fn) {
        FILE *f;
        f = romfopen(fn, "rb");
        if (f) {
                fclose(f);
                return 1;
        }
        return 0;
}

uint8_t rom_read(uint32_t addr, void *p) {
        rom_t *rom = (rom_t *)p;
        //        pclog("rom_read : %08x %08x %02x\n", addr, rom->mask, rom->rom[addr & rom->mask]);
        return rom->rom[addr & rom->mask];
}
uint16_t rom_readw(uint32_t addr, void *p) {
        rom_t *rom = (rom_t *)p;
        //        pclog("rom_readw: %08x %08x %04x\n", addr, rom->mask, *(uint16_t *)&rom->rom[addr & rom->mask]);
        return *(uint16_t *)&rom->rom[addr & rom->mask];
}
uint32_t rom_readl(uint32_t addr, void *p) {
        rom_t *rom = (rom_t *)p;
        //        pclog("rom_readl: %08x %08x %08x\n", addr, rom->mask, *(uint32_t *)&rom->rom[addr & rom->mask]);
        return *(uint32_t *)&rom->rom[addr & rom->mask];
}

int rom_init(rom_t *rom, char *fn, uint32_t address, int size, int mask, int file_offset, uint32_t flags) {
        FILE *f = romfopen(fn, "rb");

        if (!f) {
                pclog("ROM image not found : %s\n", fn);
                error("Failed to open ROM image %s\n", fn);
                return -1;
        }

        pclog("Loading ROM image : %s\n", fn);

        /* Verify file size to detect truncated ROMs */
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        if (file_size < file_offset + size) {
                fclose(f);
                pclog("ROM image %s is too small: %ld bytes, need %d\n", fn,
                      file_size, file_offset + size);
                error("ROM image %s is too small: %ld bytes, need %d\n", fn,
                      file_size, file_offset + size);
                return -1;
        }
        fseek(f, file_offset, SEEK_SET);

#if defined(_WIN32) && defined(USE_WHPX)
        /* WHPX requires guest memory to be page aligned and executable. */
        rom->rom = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                               PAGE_EXECUTE_READWRITE);
#else
        rom->rom = malloc(size);
#endif
        fread(rom->rom, size, 1, f);
        fclose(f);

        rom->mask = mask;

        mem_mapping_add(&rom->mapping, address, size, rom_read, rom_readw, rom_readl, mem_write_null, mem_write_nullw,
                        mem_write_nulll, rom->rom, flags | MEM_MAPPING_ROM, rom);
#ifdef USE_WHPX
        if (cpu_backend == CPU_BACKEND_WHPX) {
                if (ram && address + size <= (unsigned)(mem_size * 1024))
                        memcpy(ram + address, rom->rom, size);
                whpx_map_range(ram + address, address, size);
        } else
#endif
                (void)0;

        return 0;
}

int rom_init_interleaved(rom_t *rom, char *fn_low, char *fn_high, uint32_t address, int size, int mask, int file_offset,
                          uint32_t flags) {
        FILE *f_low = romfopen(fn_low, "rb");
        if (!f_low) {
                pclog("ROM image not found : %s\n", fn_low);
                error("Failed to open ROM image %s\n", fn_low);
                return -1;
        }

        FILE *f_high = romfopen(fn_high, "rb");
        if (!f_high) {
                pclog("ROM image not found : %s\n", fn_high);
                error("Failed to open ROM image %s\n", fn_high);
                fclose(f_low); /* Close any file opened successfully before returning */
                return -1;
        }

        int c;

        /* Ensure interleaved ROM parts are large enough */
        fseek(f_low, 0, SEEK_END);
        long file_size_low = ftell(f_low);
        fseek(f_high, 0, SEEK_END);
        long file_size_high = ftell(f_high);
        int part_size = file_offset + size / 2;
        if (file_size_low < part_size || file_size_high < part_size) {
                pclog("ROM image %s or %s is too small: low=%ld high=%ld, need %d\n",
                      fn_low, fn_high, file_size_low, file_size_high, part_size);
                error("ROM image %s or %s is too small: low=%ld high=%ld, need %d\n",
                      fn_low, fn_high, file_size_low, file_size_high, part_size);
                fclose(f_low);
                fclose(f_high);
                return -1;
        }
        fseek(f_low, file_offset, SEEK_SET);
        fseek(f_high, file_offset, SEEK_SET);

#if defined(_WIN32) && defined(USE_WHPX)
        rom->rom = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                               PAGE_EXECUTE_READWRITE);
#else
        rom->rom = malloc(size);
#endif
        for (c = 0; c < size; c += 2) {
                rom->rom[c] = getc(f_low);
                rom->rom[c + 1] = getc(f_high);
        }
        fclose(f_high);
        fclose(f_low);

        rom->mask = mask;

        mem_mapping_add(&rom->mapping, address, size, rom_read, rom_readw, rom_readl, mem_write_null, mem_write_nullw,
                        mem_write_nulll, rom->rom, flags | MEM_MAPPING_ROM, rom);
#ifdef USE_WHPX
        if (cpu_backend == CPU_BACKEND_WHPX) {
                if (ram && address + size <= (unsigned)(mem_size * 1024))
                        memcpy(ram + address, rom->rom, size);
                whpx_map_range(ram + address, address, size);
        } else
#endif
                (void)0;

        return 0;
}

void rom_deinit(rom_t *rom)
{
    assert(rom);

    mem_mapping_remove(&rom->mapping);
}
