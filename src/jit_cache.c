#include "jit_cache.h"
#include "config.h"
#include "paths.h"
#include "codegen.h"
#include "codegen_allocator.h"
#include "codegen_backend.h"
#include <stdio.h>
#include <string.h>

void jit_cache_load(const char *path)
{
    char fullpath[512];
    if (!path)
    {
        append_filename(fullpath, configs_path, JIT_CACHE_FILENAME, 511);
        path = fullpath;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
        return;

    uint32_t magic, version;
    fread(&magic, sizeof(magic), 1, f);
    fread(&version, sizeof(version), 1, f);
    if (magic != 0x4a495443 || version != 1)
    {
        fclose(f);
        return;
    }

    codegen_allocator_state_load(f);
    codegen_state_load(f);

    fclose(f);
}

void jit_cache_save(const char *path)
{
    char fullpath[512];
    if (!path)
    {
        append_filename(fullpath, configs_path, JIT_CACHE_FILENAME, 511);
        path = fullpath;
    }

    FILE *f = fopen(path, "wb");
    if (!f)
        return;

    uint32_t magic = 0x4a495443;
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);

    codegen_allocator_state_save(f);
    codegen_state_save(f);

    fclose(f);
}
