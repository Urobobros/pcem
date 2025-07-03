#include "ibm.h"
#include "kvm.h"
#include "x86.h"
#include "mem.h"
#include "pic.h"

#ifdef USE_KVM
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static int kvm_fd = -1;
static int vm_fd = -1;
static int vcpu_fd = -1;
static struct kvm_run *kvm_run_area;
static size_t kvm_run_size;

int kvm_available(void)
{
    int fd = open("/dev/kvm", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;

    int ver = ioctl(fd, KVM_GET_API_VERSION, 0);
    close(fd);
    return ver == KVM_API_VERSION;
}

static int sync_state_to_kvm(void)
{
    struct kvm_regs regs = {
        .rax = EAX,
        .rbx = EBX,
        .rcx = ECX,
        .rdx = EDX,
        .rsi = ESI,
        .rdi = EDI,
        .rsp = ESP,
        .rbp = EBP,
        .r8  = 0,
        .r9  = 0,
        .r10 = 0,
        .r11 = 0,
        .r12 = 0,
        .r13 = 0,
        .r14 = 0,
        .r15 = 0,
        .rip = cpu_state.pc,
        .rflags = cpu_state.eflags,
    };

    if (ioctl(vcpu_fd, KVM_SET_REGS, &regs) < 0)
        return -1;

    struct kvm_sregs sregs;
    if (ioctl(vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
        return -1;

    sregs.cs.base = cs;  sregs.cs.selector = CS;
    sregs.ds.base = ds;  sregs.ds.selector = DS;
    sregs.es.base = es;  sregs.es.selector = ES;
    sregs.ss.base = ss;  sregs.ss.selector = SS;
    sregs.fs.base = fs;  sregs.fs.selector = FS;
    sregs.gs.base = gs;  sregs.gs.selector = GS;

    sregs.cr0 = cr0;
    sregs.cr2 = cr2;
    sregs.cr3 = cr3;
    sregs.cr4 = cr4;

    sregs.gdt.base = gdt.base;
    sregs.gdt.limit = gdt.limit;
    sregs.idt.base = idt.base;
    sregs.idt.limit = idt.limit;
    sregs.tr.base = tr.base; sregs.tr.selector = tr.seg; sregs.tr.limit = tr.limit;
    sregs.ldt.base = ldt.base; sregs.ldt.selector = ldt.seg; sregs.ldt.limit = ldt.limit;

    if (ioctl(vcpu_fd, KVM_SET_SREGS, &sregs) < 0)
        return -1;

    return 0;
}

static void sync_state_from_kvm(void)
{
    struct kvm_regs regs;
    if (ioctl(vcpu_fd, KVM_GET_REGS, &regs) < 0)
        return;

    EAX = regs.rax;
    EBX = regs.rbx;
    ECX = regs.rcx;
    EDX = regs.rdx;
    ESI = regs.rsi;
    EDI = regs.rdi;
    ESP = regs.rsp;
    EBP = regs.rbp;
    cpu_state.pc = regs.rip;
    cpu_state.eflags = regs.rflags;

    struct kvm_sregs sregs;
    if (ioctl(vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
        return;

    cs = sregs.cs.base; CS = sregs.cs.selector;
    ds = sregs.ds.base; DS = sregs.ds.selector;
    es = sregs.es.base; ES = sregs.es.selector;
    ss = sregs.ss.base; SS = sregs.ss.selector;
    fs = sregs.fs.base; FS = sregs.fs.selector;
    gs = sregs.gs.base; GS = sregs.gs.selector;

    cr0 = sregs.cr0;
    cr2 = sregs.cr2;
    cr3 = sregs.cr3;
    cr4 = sregs.cr4;
}

int kvm_init(void)
{
    kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm_fd < 0)
        return -1;

    int ver = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (ver != KVM_API_VERSION)
        goto error;

    vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    if (vm_fd < 0)
        goto error;

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .flags = 0,
        .guest_phys_addr = 0,
        .memory_size = mem_size * 1024ULL,
        .userspace_addr = (uint64_t)ram,
    };

    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0)
        goto error;

    struct kvm_userspace_memory_region bios = {
        .slot = 1,
        .flags = KVM_MEM_READONLY,
        .guest_phys_addr = 0xe0000,
        .memory_size = 0x20000,
        .userspace_addr = (uint64_t)(rom + (0x20000 & biosmask)),
    };

    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &bios) < 0)
        goto error;

    vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd < 0)
        goto error;

    kvm_run_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    kvm_run_area = mmap(NULL, kvm_run_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);
    if (kvm_run_area == MAP_FAILED)
        goto error;

    if (sync_state_to_kvm() < 0)
        goto error;

    pclog("KVM initialized\n");
    return 0;

error:
    kvm_shutdown();
    return -1;
}

void kvm_shutdown(void)
{
    if (vcpu_fd >= 0) close(vcpu_fd);
    if (vm_fd >= 0) close(vm_fd);
    if (kvm_fd >= 0) close(kvm_fd);
    if (kvm_run_area)
        munmap(kvm_run_area, kvm_run_size);
    kvm_run_area = NULL;
    kvm_fd = vm_fd = vcpu_fd = -1;
}

static int kvm_handle_io(void)
{
    uint8_t *data = (uint8_t *)kvm_run_area + kvm_run_area->io.data_offset;
    uint16_t port = kvm_run_area->io.port;
    uint32_t count = kvm_run_area->io.count;
    int size = kvm_run_area->io.size;

    while (count--) {
        if (kvm_run_area->io.direction == KVM_EXIT_IO_OUT) {
            switch (size) {
            case 1:
                outb(port, *(uint8_t *)data);
                break;
            case 2:
                outw(port, *(uint16_t *)data);
                break;
            case 4:
                outl(port, *(uint32_t *)data);
                break;
            default:
                pclog("Unsupported KVM I/O size %d\n", size);
                return -1;
            }
        } else {
            switch (size) {
            case 1:
                *(uint8_t *)data = inb(port);
                break;
            case 2:
                *(uint16_t *)data = inw(port);
                break;
            case 4:
                *(uint32_t *)data = inl(port);
                break;
            default:
                pclog("Unsupported KVM I/O size %d\n", size);
                return -1;
            }
        }
        data += size;
    }

    return 0;
}

static int kvm_handle_mmio(void)
{
    uint64_t addr = kvm_run_area->mmio.phys_addr;
    uint8_t *data = kvm_run_area->mmio.data;
    uint32_t len = kvm_run_area->mmio.len;

    if (kvm_run_area->mmio.is_write) {
        for (uint32_t i = 0; i < len; i++)
            mem_writeb_phys(addr + i, data[i]);
    } else {
        for (uint32_t i = 0; i < len; i++)
            data[i] = mem_readb_phys(addr + i);
    }

    return 0;
}

static const char *kvm_exit_str(int reason)
{
    switch (reason) {
    case KVM_EXIT_HLT:
        return "HLT";
    case KVM_EXIT_IO:
        return "IO";
    case KVM_EXIT_MMIO:
        return "MMIO";
    case KVM_EXIT_IRQ_WINDOW_OPEN:
        return "IRQ_WINDOW_OPEN";
    case KVM_EXIT_FAIL_ENTRY:
        return "FAIL_ENTRY";
    case KVM_EXIT_INTERNAL_ERROR:
        return "INTERNAL_ERROR";
    default:
        return "UNKNOWN";
    }
}

int kvm_run(int cycles)
{
    (void)cycles;

    if (sync_state_to_kvm() < 0)
        return -1;

    while (1) {
        if (ioctl(vcpu_fd, KVM_RUN, 0) < 0) {
            pclog("KVM_RUN failed\n");
            return -1;
        }

        sync_state_from_kvm();

        int reason = kvm_run_area->exit_reason;
        pclog("KVM exit %s (%d)\n", kvm_exit_str(reason), reason);
        switch (reason) {
        case KVM_EXIT_HLT:
            return 0;
        case KVM_EXIT_IO:
            if (kvm_handle_io() < 0)
                return -1;
            if (sync_state_to_kvm() < 0)
                return -1;
            break;
        case KVM_EXIT_MMIO:
            if (kvm_handle_mmio() < 0)
                return -1;
            if (sync_state_to_kvm() < 0)
                return -1;
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (pic_intpending) {
                uint8_t vector = picinterrupt();
                struct kvm_interrupt intr = { .irq = vector };
                if (ioctl(vcpu_fd, KVM_INTERRUPT, &intr) < 0)
                    return -1;
            }
            if (sync_state_to_kvm() < 0)
                return -1;
            break;
        default:
            pclog("Unhandled KVM exit %s (%d)\n", kvm_exit_str(reason), reason);
            return -1;
        }
    }
}

#else
int kvm_init(void) { return -1; }
void kvm_shutdown(void) {}
int kvm_run(int cycles) { (void)cycles; return -1; }
int kvm_available(void) { return 0; }
#endif
