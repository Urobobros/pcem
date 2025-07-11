#ifndef PCEM_CPU_BACKEND_H
#define PCEM_CPU_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CPU_BACKEND_RECOMP = 0,
    CPU_BACKEND_WHPX
} CPUBackend;

extern CPUBackend cpu_backend;

void cpu_backend_set(CPUBackend backend);
void cpu_backend_init(void);
void cpu_backend_exec(int cycle_count);
void cpu_backend_shutdown(void);
void cpu_backend_memory_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PCEM_CPU_BACKEND_H */
