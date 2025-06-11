#include "jit_cache.h"
#include "config.h"
#include "paths.h"
#include "codegen.h"
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

    /* TODO: implement loading of JIT cache */

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

    /* TODO: implement saving of JIT cache */

    fclose(f);
}
