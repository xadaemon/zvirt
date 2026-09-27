/*
 * Raw wrappers around the Linux KVM API: the system handle. Port of kvm/root.zig.
 *
 * The kvm layer is a thin translation of ioctls into functions. It never logs; callers decide what
 * an error means.
 */
#ifndef ZV_KVM_KVM_H
#define ZV_KVM_KVM_H

#include "kvm/vm.h"

#include <stddef.h>

/* The only KVM API version that exists; the kernel has reported 12 since 2007. */
#define ZV_KVM_EXPECTED_API_VERSION 12

/* An open /dev/kvm. */
struct zv_kvm {
    int fd;
    /* Size of the per-vCPU `struct kvm_run` area that each vCPU fd must be mmapped with. */
    size_t vcpu_mmap_size;
};

/* Opens /dev/kvm. Returns 0, -ENOTSUP if the API version isn't 12, or another -errno. */
int zv_kvm_init(struct zv_kvm *out);

/*
 * Closes /dev/kvm. The VMM keeps the handle for the whole process (see the kvm_system getter in
 * vmm), so only tests call this.
 */
void zv_kvm_deinit(struct zv_kvm *kvm);

/* Creates a new, empty VM. Returns 0 or -errno. */
int zv_kvm_create_vm(const struct zv_kvm *kvm, struct zv_kvm_vm *out);

#endif
