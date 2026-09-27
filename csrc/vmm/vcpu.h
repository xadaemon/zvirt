/*
 * Virtual CPU: a KVM vCPU plus the thread that runs it. Port of vmm/vcpu.zig.
 *
 * The thread waits for zv_vcpu_start(), then runs the guest and handles its I/O and MMIO exits
 * until the guest stops or zv_vcpu_stop() is called. When it ends, it notifies `eventfd` so the
 * VM's main loop can see why (zv_vcpu_get_exit_reason).
 */
#ifndef ZV_VMM_VCPU_H
#define ZV_VMM_VCPU_H

#include "kvm/vcpu.h"
#include "utils/eventfd.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct zv_vm;

enum zv_vcpu_exit_reason {
    ZV_VCPU_EXIT_SHUTDOWN,
    ZV_VCPU_EXIT_UNHANDLED_EXCEPTION,
    /* The guest wrote to the test-exit port. */
    ZV_VCPU_EXIT_TEST_EXIT,
    /* zv_vcpu_stop() was called. */
    ZV_VCPU_EXIT_ABORTED,
    /* A syscall failed, or KVM reported an exit this VMM doesn't handle. */
    ZV_VCPU_EXIT_INTERNAL_ERROR,
    /* Still running. */
    ZV_VCPU_EXIT_NONE,
};

struct zv_vcpu {
    struct zv_kvm_vcpu cpu;
    size_t num;
    struct zv_vm *vm;
    pthread_t thread;

    /* The start event (std.Io.Event in Zig): `started` set under `start_lock`. */
    pthread_mutex_t start_lock;
    pthread_cond_t start_cond;
    bool started;

    atomic_bool stop_flag;
    /* Notified once, when the thread ends. */
    struct zv_eventfd eventfd;
    /* An enum zv_vcpu_exit_reason. */
    atomic_int exit_reason;
};

/*
 * Creates vCPU `num` and starts its thread, which waits for zv_vcpu_start(). vCPU 0 (the boot
 * CPU) is set up to enter the kernel at `entry_point`; the others are started by the guest.
 * Returns 0 or -errno.
 */
int zv_vcpu_new(struct zv_vm *vm, uint32_t num, uint64_t entry_point, struct zv_vcpu **out);

/* Lets the thread start running the guest. */
void zv_vcpu_start(struct zv_vcpu *vcpu);

/*
 * Asks the thread to stop: makes KVM_RUN return and kicks the thread with SIGUSR1. Safe to call
 * from any thread, more than once.
 */
void zv_vcpu_stop(struct zv_vcpu *vcpu);

enum zv_vcpu_exit_reason zv_vcpu_get_exit_reason(const struct zv_vcpu *vcpu);

/* Stops and joins the thread, then frees the vCPU. */
void zv_vcpu_deinit(struct zv_vcpu *vcpu);

#endif
