#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#define JIT_CACHE_FILENAME "jit_cache.bin"

void jit_cache_load(const char *path);
void jit_cache_save(const char *path);

#endif /* JIT_CACHE_H */
