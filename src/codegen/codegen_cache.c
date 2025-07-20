#include "codegen_cache.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_ENTRIES 64

typedef struct {
    uint32_t crc;
    size_t orig_size;
    uint8_t *orig;
    uint8_t *compiled;
    size_t compiled_size;
    int last_pos;
    int valid;
} cache_entry_t;

static cache_entry_t cache[CACHE_ENTRIES];
static int next_entry = 0;

void codegen_cache_init() {
    memset(cache, 0, sizeof(cache));
    next_entry = 0;
}

void codegen_cache_free() {
    for (int i = 0; i < CACHE_ENTRIES; i++) {
        free(cache[i].orig);
        free(cache[i].compiled);
        cache[i].orig = NULL;
        cache[i].compiled = NULL;
        cache[i].valid = 0;
    }
}

int codegen_cache_lookup(uint32_t crc, const uint8_t *orig, size_t orig_size,
                         struct mem_block_t *block, int *last_pos) {
    for (int i = 0; i < CACHE_ENTRIES; i++) {
        cache_entry_t *e = &cache[i];
        if (e->valid && e->crc == crc && e->orig_size == orig_size &&
            memcmp(e->orig, orig, orig_size) == 0) {
            codeblock_allocator_write(block, e->last_pos, e->compiled);
            if (last_pos)
                *last_pos = e->last_pos;
            return (int)e->compiled_size;
        }
    }
    return 0;
}

void codegen_cache_store(uint32_t crc, const uint8_t *orig, size_t orig_size,
                         const uint8_t *compiled, size_t compiled_size, int last_pos) {
    cache_entry_t *e = &cache[next_entry];
    free(e->orig);
    free(e->compiled);

    e->crc = crc;
    e->orig_size = orig_size;
    e->orig = malloc(orig_size);
    if (e->orig)
        memcpy(e->orig, orig, orig_size);

    e->compiled_size = compiled_size;
    e->compiled = malloc(compiled_size);
    if (e->compiled)
        memcpy(e->compiled, compiled, compiled_size);

    e->last_pos = last_pos;
    e->valid = 1;

    next_entry = (next_entry + 1) % CACHE_ENTRIES;
}
