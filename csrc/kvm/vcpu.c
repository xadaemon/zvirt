/*
 * Raw KVM vCPU descriptor wrapper. Port of kvm/vcpu.zig.
 */
#include "kvm/vcpu.h"

#include "kvm/ioctl.h"
#include "test_utils/mmap.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int zv_kvm_vcpu_init(struct zv_kvm_vcpu *vcpu, int fd, size_t mmap_size, size_t id)
{
    void *run_mapping = NULL;
    int err = zv_mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0, &run_mapping);

    if (err < 0)
        return err;

    vcpu->fd = fd;
    vcpu->run = run_mapping;
    vcpu->run_mapping_size = mmap_size;
    vcpu->id = id;
    return 0;
}

void zv_kvm_vcpu_deinit(struct zv_kvm_vcpu *vcpu)
{
    zv_munmap(vcpu->run, vcpu->run_mapping_size);
    vcpu->run = NULL;

    int rc = close(vcpu->fd);

    assert(rc == 0);
    (void)rc;
    vcpu->fd = -1;
}

int zv_kvm_vcpu_get_regs(const struct zv_kvm_vcpu *vcpu, struct kvm_regs *regs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_GET_REGS, (unsigned long)regs));
}

int zv_kvm_vcpu_set_regs(const struct zv_kvm_vcpu *vcpu, const struct kvm_regs *regs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_SET_REGS, (unsigned long)regs));
}

int zv_kvm_vcpu_get_sregs(const struct zv_kvm_vcpu *vcpu, struct kvm_sregs *sregs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_GET_SREGS, (unsigned long)sregs));
}

int zv_kvm_vcpu_set_sregs(const struct zv_kvm_vcpu *vcpu, const struct kvm_sregs *sregs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_SET_SREGS, (unsigned long)sregs));
}

int zv_kvm_vcpu_get_sregs2(const struct zv_kvm_vcpu *vcpu, struct kvm_sregs2 *sregs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_GET_SREGS2, (unsigned long)sregs));
}

int zv_kvm_vcpu_set_sregs2(const struct zv_kvm_vcpu *vcpu, const struct kvm_sregs2 *sregs)
{
    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_SET_SREGS2, (unsigned long)sregs));
}

static int decode_io_exit(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_io_exit *out)
{
    const struct kvm_run *run = vcpu->run;

    if (run->io.direction != KVM_EXIT_IO_OUT && run->io.direction != KVM_EXIT_IO_IN)
        return -ENOTSUP;

    out->direction = (enum zv_kvm_io_direction)run->io.direction;
    out->size = run->io.size;
    out->port = run->io.port;
    out->count = run->io.count;
    /* The data sits inside the kvm_run area, `data_offset` bytes from its start. */
    out->data = (uint8_t *)vcpu->run + run->io.data_offset;
    return 0;
}

static void decode_mmio_exit(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_mmio_exit *out)
{
    const struct kvm_run *run = vcpu->run;

    out->physical_address = run->mmio.phys_addr;
    /* run->mmio.data is a byte array; copy all 8 bytes rather than casting the pointer. */
    memcpy(&out->data, run->mmio.data, sizeof(out->data));
    out->is_write = run->mmio.is_write == 1;
    out->length = run->mmio.len;
}

int zv_kvm_vcpu_exit_reason(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_exit *out)
{
    switch (vcpu->run->exit_reason) {
    case KVM_EXIT_HLT:
        out->kind = ZV_KVM_EXIT_HALT;
        return 0;
    case KVM_EXIT_SHUTDOWN:
        out->kind = ZV_KVM_EXIT_SHUTDOWN;
        return 0;
    case KVM_EXIT_INTR:
        out->kind = ZV_KVM_EXIT_INTERRUPTED;
        return 0;
    case KVM_EXIT_IO:
        out->kind = ZV_KVM_EXIT_IO;
        return decode_io_exit(vcpu, &out->io);
    case KVM_EXIT_MMIO:
        out->kind = ZV_KVM_EXIT_MMIO;
        decode_mmio_exit(vcpu, &out->mmio);
        return 0;
    default:
        return -ENOTSUP;
    }
}

void zv_kvm_vcpu_immediate_exit(struct zv_kvm_vcpu *vcpu)
{
    /*
     * Another thread may be inside KVM_RUN. immediate_exit is a plain __u8 in the uapi struct, so
     * the C11 atomic_* functions (which need an _Atomic type) don't apply; use the builtin.
     */
    __atomic_store_n(&vcpu->run->immediate_exit, 1, __ATOMIC_RELAXED);
}

int zv_kvm_vcpu_run_once(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_io_result result)
{
    switch (result.kind) {
    case ZV_KVM_IO_RESULT_NONE:
        break;
    case ZV_KVM_IO_RESULT_MMIO:
        memcpy(vcpu->run->mmio.data, &result.mmio_data, sizeof(result.mmio_data));
        break;
    }

    return zv_ioctl_status(zv_ioctl(vcpu->fd, KVM_RUN, 0));
}
