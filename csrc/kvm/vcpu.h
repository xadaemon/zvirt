/*
 * Raw KVM vCPU descriptor wrapper. Port of kvm/vcpu.zig.
 */
#ifndef ZV_KVM_VCPU_H
#define ZV_KVM_VCPU_H

#include <linux/kvm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Why KVM_RUN returned. Only the exits this VMM handles are decoded. */
enum zv_kvm_exit_kind {
    ZV_KVM_EXIT_HALT,
    ZV_KVM_EXIT_IO,
    ZV_KVM_EXIT_MMIO,
    ZV_KVM_EXIT_SHUTDOWN,
    ZV_KVM_EXIT_INTERRUPTED,
};

enum zv_kvm_io_direction {
    ZV_KVM_IO_OUT = KVM_EXIT_IO_OUT,
    ZV_KVM_IO_IN = KVM_EXIT_IO_IN,
};

/*
 * A port I/O exit. `size * count` bytes live at `data` (count > 1 for string I/O like `rep outsb`).
 */
struct zv_kvm_io_exit {
    enum zv_kvm_io_direction direction;
    uint8_t size;
    uint16_t port;
    uint32_t count;
    /* Points into the vCPU's kvm_run area. For IN, the VMM writes the result here. */
    uint8_t *data;
};

/*
 * A memory-mapped I/O exit: the guest accessed `length` bytes at an address with no RAM behind it.
 */
struct zv_kvm_mmio_exit {
    uint64_t physical_address;
    /* For writes: the bytes written, little-endian. For reads: unused. */
    uint64_t data;
    bool is_write;
    size_t length;
};

struct zv_kvm_exit {
    enum zv_kvm_exit_kind kind;
    /* Only the member matching `kind` is valid. HALT, SHUTDOWN and INTERRUPTED carry no data. */
    union {
        struct zv_kvm_io_exit io;
        struct zv_kvm_mmio_exit mmio;
    };
};

enum zv_kvm_io_result_kind {
    ZV_KVM_IO_RESULT_NONE,
    ZV_KVM_IO_RESULT_MMIO,
};

/*
 * What the VMM hands back to the guest on the next KVM_RUN. After an MMIO read exit, `mmio_data`
 * holds the value the guest reads. The Zig version is an optional (?IoResult); NONE plays the role
 * of null.
 */
struct zv_kvm_io_result {
    enum zv_kvm_io_result_kind kind;
    uint64_t mmio_data;
};

struct zv_kvm_vcpu {
    int fd;
    /* The kernel-shared kvm_run area, mmapped from the vCPU fd. */
    struct kvm_run *run;
    size_t run_mapping_size;
    size_t id;
};

/* Takes ownership of `fd` only on success. Returns 0 or -errno. */
int zv_kvm_vcpu_init(struct zv_kvm_vcpu *vcpu, int fd, size_t mmap_size, size_t id);

void zv_kvm_vcpu_deinit(struct zv_kvm_vcpu *vcpu);

int zv_kvm_vcpu_get_regs(const struct zv_kvm_vcpu *vcpu, struct kvm_regs *regs);
int zv_kvm_vcpu_set_regs(const struct zv_kvm_vcpu *vcpu, const struct kvm_regs *regs);
int zv_kvm_vcpu_get_sregs(const struct zv_kvm_vcpu *vcpu, struct kvm_sregs *sregs);
int zv_kvm_vcpu_set_sregs(const struct zv_kvm_vcpu *vcpu, const struct kvm_sregs *sregs);
int zv_kvm_vcpu_get_sregs2(const struct zv_kvm_vcpu *vcpu, struct kvm_sregs2 *sregs);
int zv_kvm_vcpu_set_sregs2(const struct zv_kvm_vcpu *vcpu, const struct kvm_sregs2 *sregs);

/*
 * Decodes why the last KVM_RUN returned. Returns 0, or -ENOTSUP for an exit reason or I/O
 * direction this VMM doesn't handle (including KVM_EXIT_INTERNAL_ERROR and KVM_EXIT_FAIL_ENTRY).
 */
int zv_kvm_vcpu_exit_reason(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_exit *out);

/*
 * Makes the current or next KVM_RUN return -EINTR without entering the guest. Safe to call from
 * another thread. Used to stop a vCPU.
 */
void zv_kvm_vcpu_immediate_exit(struct zv_kvm_vcpu *vcpu);

/*
 * Delivers `result` (e.g. the value of an MMIO read) and runs the guest until the next exit.
 * Returns 0, -EINTR if interrupted by a signal or immediate exit, -EAGAIN, or another -errno.
 */
int zv_kvm_vcpu_run_once(const struct zv_kvm_vcpu *vcpu, struct zv_kvm_io_result result);

#endif
