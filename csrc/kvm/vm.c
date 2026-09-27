/*
 * Raw KVM virtual-machine descriptor wrapper. Port of kvm/vm.zig.
 *
 * Every uapi struct starts zeroed ({0}) and then has its fields assigned, so padding and reserved
 * fields are always zero.
 *
 * Unlike the Zig version, zv_kvm_vm_create_vcpu() closes the new vCPU fd when a later step fails.
 */
#include "kvm/vm.h"

#include "kvm/cpuid.h"
#include "kvm/ioctl.h"

#include <assert.h>
#include <linux/kvm.h>
#include <unistd.h>

void zv_kvm_vm_init(struct zv_kvm_vm *vm, int fd, int kvm_fd, size_t vcpu_mmap_size)
{
    vm->fd = fd;
    vm->kvm_fd = kvm_fd;
    vm->vcpu_mmap_size = vcpu_mmap_size;
}

void zv_kvm_vm_deinit(struct zv_kvm_vm *vm)
{
    int rc = close(vm->fd);

    assert(rc == 0);
    (void)rc;
    vm->fd = -1;
}

int zv_kvm_vm_set_user_memory_region(const struct zv_kvm_vm *vm, uint64_t guest_address,
                                     uint32_t slot, void *memory, size_t size)
{
    struct kvm_userspace_memory_region region = {0};

    region.slot = slot;
    region.flags = 0;
    region.guest_phys_addr = guest_address;
    region.memory_size = size;
    region.userspace_addr = (uint64_t)(uintptr_t)memory;

    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, (unsigned long)&region));
}

int zv_kvm_vm_create_pit(const struct zv_kvm_vm *vm)
{
    struct kvm_pit_config config = {0};

    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_CREATE_PIT2, (unsigned long)&config));
}

int zv_kvm_vm_create_irqchip(const struct zv_kvm_vm *vm)
{
    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_CREATE_IRQCHIP, 0));
}

int zv_kvm_vm_register_irq(const struct zv_kvm_vm *vm, const struct zv_eventfd *eventfd,
                           uint32_t gsi)
{
    struct kvm_irqfd irqfd = {0};

    irqfd.fd = (uint32_t)zv_eventfd_as_fd(eventfd);
    irqfd.gsi = gsi;

    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_IRQFD, (unsigned long)&irqfd));
}

int zv_kvm_vm_register_ioevent(const struct zv_kvm_vm *vm, const struct zv_eventfd *eventfd,
                               uint64_t address, uint32_t length, uint64_t datamatch)
{
    struct kvm_ioeventfd ioeventfd = {0};

    ioeventfd.datamatch = datamatch;
    ioeventfd.addr = address;
    ioeventfd.len = length;
    ioeventfd.fd = zv_eventfd_as_fd(eventfd);
    ioeventfd.flags = KVM_IOEVENTFD_FLAG_DATAMATCH;

    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_IOEVENTFD, (unsigned long)&ioeventfd));
}

int zv_kvm_vm_irq_set(const struct zv_kvm_vm *vm, uint32_t irq, bool set)
{
    struct kvm_irq_level level = {0};

    level.irq = irq;
    level.level = set ? 1 : 0;

    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_IRQ_LINE, (unsigned long)&level));
}

int zv_kvm_vm_msi_signal(const struct zv_kvm_vm *vm, uint32_t address_lo, uint32_t address_hi,
                         uint32_t data)
{
    struct kvm_msi msi = {0};

    msi.address_lo = address_lo;
    msi.address_hi = address_hi;
    msi.data = data;
    msi.flags = 0;
    msi.devid = 0;

    /*
     * KVM_SIGNAL_MSI returns 1 when the interrupt was delivered and 0 when the guest blocked it.
     * Both count as success, as in the Zig version.
     */
    return zv_ioctl_status(zv_ioctl(vm->fd, KVM_SIGNAL_MSI, (unsigned long)&msi));
}

int zv_kvm_vm_create_vcpu(const struct zv_kvm_vm *vm, uint32_t id, uint32_t num_cpus,
                          struct zv_kvm_vcpu *out)
{
    int vcpu_fd = zv_ioctl(vm->fd, KVM_CREATE_VCPU, id);

    if (vcpu_fd < 0)
        return vcpu_fd;

    int err = zv_kvm_vcpu_init(out, vcpu_fd, vm->vcpu_mmap_size, id);

    if (err < 0) {
        close(vcpu_fd);
        return err;
    }

    /* From here on the vCPU owns the fd, so zv_kvm_vcpu_deinit() closes it. */
    err = zv_kvm_cpuid_configure(vm->kvm_fd, id, num_cpus, out->fd);

    if (err < 0) {
        zv_kvm_vcpu_deinit(out);
        return err;
    }

    return 0;
}
