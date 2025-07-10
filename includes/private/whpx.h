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
int whpx_map_rom(const void *mem, unsigned long long gpa, size_t size);
int whpx_map_range(void *mem, unsigned long long gpa, size_t size);
int whpx_map_vga_memory(void *mem);
void *whpx_get_ram_base(void);
size_t whpx_get_ram_size(void);
int whpx_reset_vcpu(void);
int whpx_unmap_range(unsigned long long gpa, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* PCEM_WHPX_H */
