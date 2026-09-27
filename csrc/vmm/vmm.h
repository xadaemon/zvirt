/*
 * The virtual machine: policy and guest setup. Port of vmm/root.zig.
 *
 * Lifecycle: zv_vm_new() builds the VM with its vCPU threads waiting. zv_vm_attach_console()
 * connects COM ports. zv_vm_run() starts the vCPUs and runs the main loop (device events) until a
 * vCPU stops, which stops the others. zv_vm_stop() can end a running VM from another thread.
 * zv_vm_deinit() tears everything down.
 */
#ifndef ZV_VMM_VMM_H
#define ZV_VMM_VMM_H

#include "kvm/kvm.h"
#include "kvm/vm.h"
#include "utils/epoll.h"
#include "utils/eventfd.h"
#include "vmm/arch/x86/vm.h"
#include "vmm/event_token.h"
#include "vmm/memory.h"
#include "vmm/vm_config.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct zv_vcpu;

struct zv_vm_console_config {
    /* Guest input comes from this fd, or -1 for none. */
    int input_fd;
    /* Guest output goes to this fd. */
    int output_fd;
    /* COM port number: 0 to 3 (COM1 to COM4). */
    uint8_t index;
    /* Put the output terminal into raw mode, and restore it on deinit. */
    bool configure_terminal;
};

enum zv_vm_state {
    ZV_VM_INITIALIZED,
    ZV_VM_RUNNING,
    ZV_VM_STOPPED,
};

struct zv_vm {
    struct zv_kvm_vm kvm_vm;
    struct zv_vcpu *vcpus[ZV_VM_MAX_VCPUS];
    size_t vcpus_count;
    /*
     * A copy of the configuration. The buffers it points to (binary, initramfs, cmdline, paths)
     * must stay valid until zv_vm_deinit(); cmdline, for example, is read by zv_vm_run().
     */
    struct zv_vm_config config;
    struct zv_guest_memory *memory;
    struct zv_epoll epoll;
    /* An enum zv_vm_state. */
    atomic_int state;
    /* The SIGUSR1 handler that was installed before this VM, restored on deinit. */
    struct sigaction old_sigaction;
    struct zv_x86_vm arch;
};

/*
 * The process-wide /dev/kvm handle, opened on first use (kvm_system in Zig). Fails with the same
 * error on every call if opening failed. Must not be called in the test runner's parent process.
 */
int zv_vmm_kvm_system(struct zv_kvm **out);

/*
 * Builds a VM: guest memory, the kernel image, devices and vCPUs. Returns 0, -EINVAL for a bad
 * config, -ENOTSUP for a device that isn't ported to C yet (PCI, block, network), or -errno.
 */
int zv_vm_new(const struct zv_vm_config *config, struct zv_vm **out);

void zv_vm_deinit(struct zv_vm *vm);

enum zv_vm_state zv_vm_get_state(const struct zv_vm *vm);

/*
 * Attaches a console to a COM port. Only before zv_vm_run(); not thread-safe. Returns 0, -EINVAL
 * if the VM has started or the index is invalid, -EEXIST if the port has a console, or -errno.
 */
int zv_vm_attach_console(struct zv_vm *vm, const struct zv_vm_console_config *console);

int zv_vm_register_irq(struct zv_vm *vm, const struct zv_eventfd *eventfd, uint32_t irq);

int zv_vm_msi_signal(struct zv_vm *vm, uint32_t address_lo, uint32_t address_hi, uint32_t data);

int zv_vm_register_ioevent(struct zv_vm *vm, const struct zv_eventfd *eventfd, uint64_t address,
                           uint32_t length, uint64_t datamatch);

/* Adds `fd` to the main loop; its events go to `source` with `id`. Returns 0 or -errno. */
int zv_vm_register_fd(struct zv_vm *vm, int fd, uint32_t id, enum zv_event_source source,
                      bool edge);

/*
 * Starts the vCPUs and runs the main loop until a vCPU stops. Returns 0 (whatever the reason the
 * guest stopped), -EALREADY if the VM was already started, or -errno.
 */
int zv_vm_run(struct zv_vm *vm);

/* Stops a running VM from another thread. Returns 0, or -EINVAL if the VM isn't running. */
int zv_vm_stop(struct zv_vm *vm);

#endif
