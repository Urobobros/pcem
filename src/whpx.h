#ifndef PCEM_WHPX_H
#define PCEM_WHPX_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

int whpx_init(void);
void whpx_deinit(void);
int whpx_vcpu_create(void);
void whpx_vcpu_destroy(void);
int whpx_vcpu_run(void);
int whpx_map_memory(void *mem, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* PCEM_WHPX_H */
