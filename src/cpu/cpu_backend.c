#include "ibm.h"
#include "cpu.h"
#include "cpu_backend.h"
#include "kvm.h"
#include "x86.h"

int cpu_backend_init(void)
{
#ifdef USE_KVM
    if (cpu_use_kvm) {
        if (!kvm_available()) {
            pclog("/dev/kvm not available\n");
            cpu_use_kvm = 0;
        } else if (kvm_init() < 0) {
            pclog("KVM init failed, falling back to emulation\n");
            cpu_use_kvm = 0;
        }
    }
#endif
    return 0;
}

void cpu_backend_shutdown(void)
{
#ifdef USE_KVM
    if (cpu_use_kvm)
        kvm_shutdown();
#endif
}

int cpu_backend_run(int cycles)
{
    if (cpu_use_kvm)
        return kvm_run(cycles);

    if (is386) {
        if (cpu_use_dynarec)
            exec386_dynarec(cycles);
        else
            exec386(cycles);
    } else if (AT)
        exec386(cycles);
    else
        execx86(cycles);

    return 0;
}
