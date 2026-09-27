/*
 * Raw KVM virtual-machine descriptor wrapper. Port of kvm/vm.zig.
 */
#ifndef ZV_KVM_VM_H
#define ZV_KVM_VM_H

#include "kvm/vcpu.h"
#include "utils/eventfd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct zv_kvm_vm {
    int fd;
    /* The /dev/kvm fd, needed to query supported CPUID leaves when creating vCPUs. */
    int kvm_fd;
    size_t vcpu_mmap_size;
};

void zv_kvm_vm_init(struct zv_kvm_vm *vm, int fd, int kvm_fd, size_t vcpu_mmap_size);

void zv_kvm_vm_deinit(struct zv_kvm_vm *vm);

/* Maps `size` bytes of host memory at `memory` into the guest at `guest_address`. */
int zv_kvm_vm_set_user_memory_region(const struct zv_kvm_vm *vm, uint64_t guest_address,
                                     uint32_t slot, void *memory, size_t size);

/* Creates the in-kernel i8254 timer (PIT). Requires the in-kernel irqchip. */
int zv_kvm_vm_create_pit(const struct zv_kvm_vm *vm);

/* Creates the in-kernel interrupt controllers (PIC, IOAPIC, and a local APIC per vCPU). */
int zv_kvm_vm_create_irqchip(const struct zv_kvm_vm *vm);

/* Makes every write to `eventfd` raise interrupt line `gsi` in the guest. */
int zv_kvm_vm_register_irq(const struct zv_kvm_vm *vm, const struct zv_eventfd *eventfd,
                           uint32_t gsi);

/*
 * Makes guest writes of `datamatch` to `address` (`length` bytes) signal `eventfd` inside the
 * kernel, without an MMIO exit to the VMM.
 */
int zv_kvm_vm_register_ioevent(const struct zv_kvm_vm *vm, const struct zv_eventfd *eventfd,
                               uint64_t address, uint32_t length, uint64_t datamatch);

/* Raises (`set` true) or lowers interrupt line `irq`. */
int zv_kvm_vm_irq_set(const struct zv_kvm_vm *vm, uint32_t irq, bool set);

/* Injects one message-signalled interrupt. */
int zv_kvm_vm_msi_signal(const struct zv_kvm_vm *vm, uint32_t address_lo, uint32_t address_hi,
                         uint32_t data);

/*
 * Creates vCPU `id` and configures its CPUID for a machine with `num_cpus` CPUs. Returns 0 or
 * -errno (-EEXIST if the id is taken).
 */
int zv_kvm_vm_create_vcpu(const struct zv_kvm_vm *vm, uint32_t id, uint32_t num_cpus,
                          struct zv_kvm_vcpu *out);

#endif
