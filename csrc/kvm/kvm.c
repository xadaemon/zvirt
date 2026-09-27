/*
 * Raw wrappers around the Linux KVM API: the system handle. Port of kvm/root.zig.
 *
 * Unlike the Zig version, zv_kvm_init() closes /dev/kvm again when a later step fails.
 */
#include "kvm/kvm.h"

#include "kvm/ioctl.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <unistd.h>

int zv_kvm_init(struct zv_kvm *out)
{
    int fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);

    if (fd < 0)
        return -errno;

    int err;
    int version = zv_ioctl(fd, KVM_GET_API_VERSION, 0);

    if (version < 0) {
        err = version;
        goto close_fd;
    }

    if (version != ZV_KVM_EXPECTED_API_VERSION) {
        err = -ENOTSUP;
        goto close_fd;
    }

    int mmap_size = zv_ioctl(fd, KVM_GET_VCPU_MMAP_SIZE, 0);

    if (mmap_size < 0) {
        err = mmap_size;
        goto close_fd;
    }

    out->fd = fd;
    out->vcpu_mmap_size = (size_t)mmap_size;
    return 0;

close_fd:
    close(fd);
    return err;
}

void zv_kvm_deinit(struct zv_kvm *kvm)
{
    int rc = close(kvm->fd);

    assert(rc == 0);
    (void)rc;
    kvm->fd = -1;
}

int zv_kvm_create_vm(const struct zv_kvm *kvm, struct zv_kvm_vm *out)
{
    int vm_fd = zv_ioctl(kvm->fd, KVM_CREATE_VM, 0);

    if (vm_fd < 0)
        return vm_fd;

    zv_kvm_vm_init(out, vm_fd, kvm->fd, kvm->vcpu_mmap_size);
    return 0;
}
