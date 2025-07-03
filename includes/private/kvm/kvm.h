#ifndef PCEM_KVM_H
#define PCEM_KVM_H

int kvm_init(void);
void kvm_shutdown(void);
int kvm_run(int cycles);
int kvm_available(void);

#endif /* PCEM_KVM_H */
