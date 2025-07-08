#include "ibm.h"
#include "cpu_backend.h"
#include "x86.h"
#include "cpu.h"
#include "mem.h"
#ifdef USE_WHPX
#include "whpx.h"
#endif

CPUBackend cpu_backend = CPU_BACKEND_RECOMP;

void cpu_backend_set(CPUBackend backend) { cpu_backend = backend; }

void cpu_backend_init(void) {
#ifdef USE_WHPX
        cpu_backend = CPU_BACKEND_WHPX;
        if (whpx_init() == 0) {
                pclog("Using WHPX backend\n");
        } else {
                cpu_backend = CPU_BACKEND_RECOMP;
                pclog("WHPX initialization failed, falling back to interpreter\n");
        }
#else
        pclog("WHPX support not compiled; using interpreter\n");
#endif
}

void cpu_backend_exec(int cycle_count) {
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
        if (is386) {
                if (cpu_use_dynarec)
                        exec386_dynarec(cycle_count);
                else
                        exec386(cycle_count);
        } else if (AT)
                exec386(cycle_count);
        else
                execx86(cycle_count);
}

void cpu_backend_memory_init(void) {
#ifdef USE_WHPX
        if (cpu_backend == CPU_BACKEND_WHPX) {
                whpx_vcpu_destroy();
                if (whpx_map_memory(ram, mem_size * 1024) != 0) {
                        pclog("whpx: memory mapping failed, falling back to interpreter\n");
                        cpu_backend = CPU_BACKEND_RECOMP;
                        return;
                }
                if (whpx_vcpu_create() != 0) {
                        pclog("whpx: vcpu creation failed, falling back to interpreter\n");
                        cpu_backend = CPU_BACKEND_RECOMP;
                }
        }
#endif
}

void cpu_backend_shutdown(void) {
#ifdef USE_WHPX
        if (cpu_backend == CPU_BACKEND_WHPX) {
                whpx_vcpu_destroy();
                whpx_deinit();
        }
#endif
}
