/*
 * Virtual CPU: a KVM vCPU plus the thread that runs it. Port of vmm/vcpu.zig.
 */
#include "vmm/vcpu.h"

#include "utils/log.h"
#include "vmm/arch/x86/vm.h"
#include "vmm/vmm.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_SCOPE "vcpu"

/* CPUID always reports 16 logical CPUs, whatever the VM's vCPU count (as in the Zig version). */
#define CPUID_CPU_COUNT 16

static void *run_thread(void *arg);

int zv_vcpu_new(struct zv_vm *vm, uint32_t num, uint64_t entry_point, struct zv_vcpu **out)
{
    struct zv_vcpu *vcpu = calloc(1, sizeof(*vcpu));

    if (vcpu == NULL)
        return -ENOMEM;

    int err = zv_kvm_vm_create_vcpu(&vm->kvm_vm, num, CPUID_CPU_COUNT, &vcpu->cpu);

    if (err < 0)
        goto free_vcpu;

    err = zv_eventfd_new(0, &vcpu->eventfd);
    if (err < 0)
        goto deinit_cpu;

    vcpu->num = num;
    vcpu->vm = vm;
    pthread_mutex_init(&vcpu->start_lock, NULL);
    pthread_cond_init(&vcpu->start_cond, NULL);
    vcpu->started = false;
    atomic_init(&vcpu->stop_flag, false);
    atomic_init(&vcpu->exit_reason, ZV_VCPU_EXIT_NONE);

    /* Only the boot CPU needs a context: the kernel sets up the others when it wakes them. */
    if (num == 0) {
        err = zv_x86_setup_bs_vcpu(&vcpu->cpu, entry_point);
        if (err < 0)
            goto destroy_sync;
    }

    err = -pthread_create(&vcpu->thread, NULL, run_thread, vcpu);
    if (err < 0)
        goto destroy_sync;

    *out = vcpu;
    return 0;

destroy_sync:
    pthread_cond_destroy(&vcpu->start_cond);
    pthread_mutex_destroy(&vcpu->start_lock);
    zv_eventfd_deinit(&vcpu->eventfd);
deinit_cpu:
    zv_kvm_vcpu_deinit(&vcpu->cpu);
free_vcpu:
    free(vcpu);
    return err;
}

void zv_vcpu_start(struct zv_vcpu *vcpu)
{
    pthread_mutex_lock(&vcpu->start_lock);
    vcpu->started = true;
    pthread_cond_broadcast(&vcpu->start_cond);
    pthread_mutex_unlock(&vcpu->start_lock);
}

enum zv_vcpu_exit_reason zv_vcpu_get_exit_reason(const struct zv_vcpu *vcpu)
{
    return (enum zv_vcpu_exit_reason)atomic_load_explicit(&vcpu->exit_reason, memory_order_relaxed);
}

void zv_vcpu_stop(struct zv_vcpu *vcpu)
{
    zv_kvm_vcpu_immediate_exit(&vcpu->cpu);
    atomic_store_explicit(&vcpu->stop_flag, true, memory_order_relaxed);

    /* Only record "aborted" if the vCPU hasn't already stopped for another reason. */
    int expected = ZV_VCPU_EXIT_NONE;

    atomic_compare_exchange_strong_explicit(&vcpu->exit_reason, &expected, ZV_VCPU_EXIT_ABORTED,
                                            memory_order_release, memory_order_relaxed);

    /* Interrupts a KVM_RUN that is already in progress. */
    pthread_kill(vcpu->thread, SIGUSR1);
}

void zv_vcpu_deinit(struct zv_vcpu *vcpu)
{
    /* A thread that was never started is waiting for the start event. */
    zv_vcpu_start(vcpu);
    zv_vcpu_stop(vcpu);
    pthread_join(vcpu->thread, NULL);

    pthread_cond_destroy(&vcpu->start_cond);
    pthread_mutex_destroy(&vcpu->start_lock);
    zv_eventfd_deinit(&vcpu->eventfd);
    zv_kvm_vcpu_deinit(&vcpu->cpu);
    free(vcpu);
}

static void set_exit_reason(struct zv_vcpu *vcpu, enum zv_vcpu_exit_reason reason)
{
    atomic_store_explicit(&vcpu->exit_reason, reason, memory_order_relaxed);
}

static void wait_for_start(struct zv_vcpu *vcpu)
{
    pthread_mutex_lock(&vcpu->start_lock);

    /* A loop, because a SIGUSR1 kick can wake the wait early. */
    while (!vcpu->started)
        pthread_cond_wait(&vcpu->start_cond, &vcpu->start_lock);

    pthread_mutex_unlock(&vcpu->start_lock);
}

/* Runs the guest until it stops. Returns 0, or -errno if a syscall or device failed. */
static int run_loop(struct zv_vcpu *vcpu)
{
    struct zv_device_bus *device_bus = &vcpu->vm->arch.device_bus;

    /*
     * The value handed back on the next KVM_RUN. As in the Zig version it only changes on MMIO
     * exits, and the last one is written back on every run; KVM ignores it unless it is due.
     */
    struct zv_kvm_io_result result = {.kind = ZV_KVM_IO_RESULT_NONE};

    while (!atomic_load_explicit(&vcpu->stop_flag, memory_order_relaxed)) {
        int err = zv_kvm_vcpu_run_once(&vcpu->cpu, result);

        /* Interrupted (the stop kick, or another signal) or asked to retry: check the flag. */
        if (err == -EINTR || err == -EAGAIN)
            continue;
        if (err < 0)
            return err;

        struct zv_kvm_exit exit_info;

        if (zv_kvm_vcpu_exit_reason(&vcpu->cpu, &exit_info) < 0) {
            zv_log_warn(LOG_SCOPE, "unknown exit reason");
            set_exit_reason(vcpu, ZV_VCPU_EXIT_INTERNAL_ERROR);
            return 0;
        }

        switch (exit_info.kind) {
        case ZV_KVM_EXIT_IO: {
            bool test_exit = false;

            err = zv_device_bus_handle_io(device_bus, &exit_info.io, &test_exit);
            if (err < 0)
                return err;

            if (test_exit) {
                set_exit_reason(vcpu, ZV_VCPU_EXIT_TEST_EXIT);
                return 0;
            }
            break;
        }
        case ZV_KVM_EXIT_SHUTDOWN:
            set_exit_reason(vcpu, ZV_VCPU_EXIT_SHUTDOWN);
            return 0;
        case ZV_KVM_EXIT_MMIO:
            err = zv_device_bus_handle_mmio(device_bus, &exit_info.mmio, &result);
            if (err < 0)
                return err;
            break;
        case ZV_KVM_EXIT_INTERRUPTED:
            break;
        case ZV_KVM_EXIT_HALT:
            /* With the in-kernel irqchip, KVM handles HLT itself. */
            fprintf(stderr, "vcpu: unexpected HLT exit\n");
            abort();
        }
    }

    return 0;
}

static void *run_thread(void *arg)
{
    struct zv_vcpu *vcpu = arg;

    wait_for_start(vcpu);

    if (run_loop(vcpu) < 0)
        set_exit_reason(vcpu, ZV_VCPU_EXIT_INTERNAL_ERROR);

    /* The vCPU is done; tell the main loop. Every path through run_loop ends here. */
    if (zv_eventfd_notify(&vcpu->eventfd) < 0) {
        fprintf(stderr, "vcpu: failed to notify the main loop\n");
        abort();
    }

    return NULL;
}
