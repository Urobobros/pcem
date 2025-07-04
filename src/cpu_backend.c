#include "ibm.h"
#include "cpu_backend.h"
#include "x86.h"
#include "cpu.h"
#include "mem.h"
#ifdef USE_WHPX
#include "whpx.h"
#endif

CPUBackend cpu_backend = CPU_BACKEND_RECOMP;

void cpu_backend_set(CPUBackend backend)
{
    cpu_backend = backend;
}

void cpu_backend_init(void)
{
#ifdef USE_WHPX
    cpu_backend = CPU_BACKEND_WHPX;
    if (whpx_init() != 0) {
        cpu_backend = CPU_BACKEND_RECOMP;
    }
#endif
}

void cpu_backend_exec(int cycles)
{
#ifdef USE_WHPX
    if (cpu_backend == CPU_BACKEND_WHPX) {
        int rc = whpx_vcpu_run();
        if (rc == 1)
            return; /* HALT */
        if (rc < 0)
            cpu_backend = CPU_BACKEND_RECOMP;
        /* rc == 0 falls through to interpreter */
    }
#endif
    if (is386)
    {
        if (cpu_use_dynarec)
            exec386_dynarec(cycles);
        else
            exec386(cycles);
    }
    else if (AT)
        exec386(cycles);
    else
        execx86(cycles);
}

void cpu_backend_memory_init(void)
{
#ifdef USE_WHPX
    if (cpu_backend == CPU_BACKEND_WHPX) {
        if (whpx_map_memory(ram, mem_size * 1024) != 0 ||
            whpx_vcpu_create() != 0) {
            cpu_backend = CPU_BACKEND_RECOMP;
        }
    }
#endif
}

void cpu_backend_shutdown(void)
{
#ifdef USE_WHPX
    if (cpu_backend == CPU_BACKEND_WHPX) {
        whpx_vcpu_destroy();
        whpx_deinit();
    }
#endif
}
