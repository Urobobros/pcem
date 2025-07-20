#ifndef PCEM_CPU_BACKEND_H
#define PCEM_CPU_BACKEND_H

int cpu_backend_init(void);
void cpu_backend_shutdown(void);
int cpu_backend_run(int cycles);

#endif /* PCEM_CPU_BACKEND_H */
