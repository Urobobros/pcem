#ifndef _CODEGEN_CACHE_H_
#define _CODEGEN_CACHE_H_
#include <stdint.h>
#include <stddef.h>
#include "codegen_allocator.h"

void codegen_cache_init();
void codegen_cache_free();
int codegen_cache_lookup(uint32_t crc, const uint8_t *orig, size_t orig_size,
                         struct mem_block_t *block, int *last_pos);
void codegen_cache_store(uint32_t crc, const uint8_t *orig, size_t orig_size,
                         const uint8_t *compiled, size_t compiled_size, int last_pos);

#endif /* _CODEGEN_CACHE_H_ */
