/*
 * The virtual machine: policy and guest setup. Port of vmm/root.zig.
 *
 * Deviation: if creating a vCPU fails, zv_vm_new() tears down the vCPUs already created (joining
 * their threads). The Zig version leaves them running with pointers into freed memory.
 */
#include "vmm/vmm.h"

#include "utils/log.h"
#include "vmm/image/image.h"
#include "vmm/vcpu.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define LOG_SCOPE "vmm"

/* ---- The process-wide KVM handle (a Lazy in Zig) ---- */

static pthread_once_t kvm_system_once = PTHREAD_ONCE_INIT;
static struct zv_kvm kvm_system;
static int kvm_system_error;

static void open_kvm_system(void)
{
    /* The result, success or failure, is kept for every later call. */
    kvm_system_error = zv_kvm_init(&kvm_system);
}

int zv_vmm_kvm_system(struct zv_kvm **out)
{
    pthread_once(&kvm_system_once, open_kvm_system);

    if (kvm_system_error < 0)
        return kvm_system_error;

    *out = &kvm_system;
    return 0;
}

/* ---- SIGUSR1: kicks vCPU threads out of KVM_RUN ---- */

static void wake_handler(int signal)
{
    (void)signal;
}

static struct sigaction setup_sighandler(void)
{
    struct sigaction action;
    struct sigaction old;

    memset(&action, 0, sizeof(action));
    action.sa_handler = wake_handler;
    sigemptyset(&action.sa_mask);
    /* No SA_RESTART: the signal must interrupt KVM_RUN. */
    action.sa_flags = 0;

    sigaction(SIGUSR1, &action, &old);
    return old;
}

static void restore_sighandler(const struct sigaction *old)
{
    sigaction(SIGUSR1, old, NULL);
}

/* ---- Creation and teardown ---- */

/* Devices that are only ported in later phases. */
static bool uses_unported_devices(const struct zv_vm_config *config)
{
    return config->pci || config->block_device.path != NULL || config->network.enabled;
}

static int create_vcpu(struct zv_vm *vm, uint64_t entry_point, uint32_t id)
{
    if (id >= ZV_VM_MAX_VCPUS)
        return -EINVAL;

    if (vm->vcpus[id] != NULL)
        return -EEXIST;

    int err = zv_vcpu_new(vm, id, entry_point, &vm->vcpus[id]);

    if (err < 0)
        return err;

    vm->vcpus_count++;
    return 0;
}

static void deinit_vcpus(struct zv_vm *vm)
{
    for (size_t i = 0; i < ZV_VM_MAX_VCPUS; i++) {
        if (vm->vcpus[i] != NULL) {
            zv_vcpu_deinit(vm->vcpus[i]);
            vm->vcpus[i] = NULL;
        }
    }
}

int zv_vm_new(const struct zv_vm_config *config, struct zv_vm **out)
{
    int err = zv_vm_config_verify(config);

    if (err < 0)
        return err;

    if (uses_unported_devices(config))
        return -ENOTSUP;

    struct zv_kvm *kvm = NULL;

    err = zv_vmm_kvm_system(&kvm);
    if (err < 0)
        return err;

    struct zv_vm *vm = calloc(1, sizeof(*vm));

    if (vm == NULL)
        return -ENOMEM;

    vm->config = *config;
    atomic_init(&vm->state, ZV_VM_INITIALIZED);

    err = zv_kvm_create_vm(kvm, &vm->kvm_vm);
    if (err < 0)
        goto free_vm;

    err = zv_kvm_vm_create_irqchip(&vm->kvm_vm);
    if (err < 0)
        goto deinit_kvm_vm;

    err = zv_guest_memory_new(&vm->memory);
    if (err < 0)
        goto deinit_kvm_vm;

    zv_x86_vm_init(&vm->arch);

    err = zv_x86_vm_setup_vm(&vm->arch, &vm->kvm_vm, vm->memory, config);
    if (err < 0)
        goto deinit_arch;

    struct zv_image image;

    err = zv_image_parse(config->binary, config->binary_size, vm->memory, config, &image);
    if (err < 0)
        goto deinit_arch;

    for (size_t i = 0; i < vm->memory->region_count; i++) {
        struct zv_memory_region *region = &vm->memory->regions[i];

        err = zv_kvm_vm_set_user_memory_region(&vm->kvm_vm, region->gpa, region->slot, region->raw,
                                               region->size);
        if (err < 0)
            goto deinit_arch;
    }

    vm->old_sigaction = setup_sighandler();

    err = zv_epoll_new(&vm->epoll);
    if (err < 0)
        goto restore_sighandler;

    err = zv_x86_vm_setup_devices(&vm->arch, config, vm);
    if (err < 0)
        goto deinit_epoll;

    /* The boot CPU enters the kernel; the others start when the guest wakes them. */
    err = create_vcpu(vm, image.entry_point, 0);
    if (err < 0)
        goto deinit_vcpus;

    for (uint32_t i = 1; i < config->smp; i++) {
        err = create_vcpu(vm, 0x0, i);
        if (err < 0)
            goto deinit_vcpus;
    }

    *out = vm;
    return 0;

deinit_vcpus:
    deinit_vcpus(vm);
deinit_epoll:
    zv_epoll_deinit(&vm->epoll);
restore_sighandler:
    restore_sighandler(&vm->old_sigaction);
deinit_arch:
    zv_x86_vm_deinit(&vm->arch);
    zv_guest_memory_deinit(vm->memory);
deinit_kvm_vm:
    zv_kvm_vm_deinit(&vm->kvm_vm);
free_vm:
    free(vm);
    return err;
}

void zv_vm_deinit(struct zv_vm *vm)
{
    /* Joins every vCPU thread, so nothing touches guest memory or devices afterwards. */
    deinit_vcpus(vm);

    zv_epoll_deinit(&vm->epoll);
    zv_x86_vm_deinit(&vm->arch);
    zv_kvm_vm_deinit(&vm->kvm_vm);
    zv_guest_memory_deinit(vm->memory);

    /* Last: stopping the vCPUs above sends SIGUSR1, which must still reach our handler. */
    restore_sighandler(&vm->old_sigaction);
    free(vm);
}

enum zv_vm_state zv_vm_get_state(const struct zv_vm *vm)
{
    return (enum zv_vm_state)atomic_load_explicit(&vm->state, memory_order_relaxed);
}

/* ---- Wiring for devices ---- */

int zv_vm_attach_console(struct zv_vm *vm, const struct zv_vm_console_config *console)
{
    if (zv_vm_get_state(vm) != ZV_VM_INITIALIZED)
        return -EINVAL;

    return zv_x86_vm_attach_console(&vm->arch, console, vm);
}

int zv_vm_register_irq(struct zv_vm *vm, const struct zv_eventfd *eventfd, uint32_t irq)
{
    return zv_kvm_vm_register_irq(&vm->kvm_vm, eventfd, irq);
}

int zv_vm_msi_signal(struct zv_vm *vm, uint32_t address_lo, uint32_t address_hi, uint32_t data)
{
    return zv_kvm_vm_msi_signal(&vm->kvm_vm, address_lo, address_hi, data);
}

int zv_vm_register_ioevent(struct zv_vm *vm, const struct zv_eventfd *eventfd, uint64_t address,
                           uint32_t length, uint64_t datamatch)
{
    return zv_kvm_vm_register_ioevent(&vm->kvm_vm, eventfd, address, length, datamatch);
}

int zv_vm_register_fd(struct zv_vm *vm, int fd, uint32_t id, enum zv_event_source source, bool edge)
{
    struct zv_event_token token = {.id = id, .fd = fd, .source = source};
    uint32_t events = EPOLLIN | (edge ? EPOLLET : 0);

    return zv_epoll_add_with_events(&vm->epoll, fd, events, zv_event_token_encode(token));
}

/* ---- Running ---- */

int zv_vm_stop(struct zv_vm *vm)
{
    int expected = ZV_VM_RUNNING;

    if (!atomic_compare_exchange_strong_explicit(&vm->state, &expected, ZV_VM_STOPPED,
                                                 memory_order_relaxed, memory_order_relaxed))
        return -EINVAL;

    for (size_t i = 0; i < ZV_VM_MAX_VCPUS; i++) {
        if (vm->vcpus[i] != NULL)
            zv_vcpu_stop(vm->vcpus[i]);
    }

    return 0;
}

/*
 * Handles one main-loop event. Sets *stopped_vcpu to the vCPU's index if a vCPU has stopped.
 * Returns 0 or -errno.
 */
static int handle_event(struct zv_vm *vm, const struct epoll_event *event, int *stopped_vcpu)
{
    struct zv_event_token token = zv_event_token_decode(event->data.u64);

    if (token.source != ZV_EVENT_SOURCE_VCPU)
        return zv_device_bus_handle_event(&vm->arch.device_bus, token.source, token.id, token.fd);

    /* Read the eventfd so the same event doesn't fire again. */
    struct zv_eventfd eventfd = {.fd = token.fd};
    uint64_t value;
    int err = zv_eventfd_read(&eventfd, &value);

    if (err < 0)
        return err;

    if (zv_vcpu_get_exit_reason(vm->vcpus[token.id]) != ZV_VCPU_EXIT_NONE)
        *stopped_vcpu = (int)token.id;

    return 0;
}

int zv_vm_run(struct zv_vm *vm)
{
    int expected = ZV_VM_INITIALIZED;

    if (!atomic_compare_exchange_strong_explicit(&vm->state, &expected, ZV_VM_RUNNING,
                                                 memory_order_relaxed, memory_order_relaxed))
        return -EALREADY;

    int err = zv_x86_vm_prerun(&vm->arch, vm);

    if (err < 0)
        return err;

    for (size_t i = 0; i < ZV_VM_MAX_VCPUS; i++) {
        struct zv_vcpu *vcpu = vm->vcpus[i];

        if (vcpu == NULL)
            continue;

        err = zv_vm_register_fd(vm, vcpu->eventfd.fd, (uint32_t)i, ZV_EVENT_SOURCE_VCPU, false);
        if (err < 0)
            return err;

        zv_vcpu_start(vcpu);
    }

    /* Block every signal while waiting, so SIGUSR1 kicks meant for vCPUs don't land here. */
    sigset_t all_signals;

    sigfillset(&all_signals);

    int stopped_vcpu = -1;

    while (stopped_vcpu < 0) {
        struct epoll_event events[1];
        int count = zv_epoll_pwait(&vm->epoll, events, 1, -1, &all_signals);

        if (count == -EINTR)
            continue;
        if (count < 0)
            return count;

        for (int i = 0; i < count && stopped_vcpu < 0; i++) {
            err = handle_event(vm, &events[i], &stopped_vcpu);
            if (err < 0)
                return err;
        }
    }

    zv_log_info(LOG_SCOPE, "vCPU %d requested stop. Stopping the guest", stopped_vcpu);

    for (size_t i = 0; i < ZV_VM_MAX_VCPUS; i++) {
        if (vm->vcpus[i] != NULL && (int)i != stopped_vcpu)
            zv_vcpu_stop(vm->vcpus[i]);
    }

    atomic_store_explicit(&vm->state, ZV_VM_STOPPED, memory_order_relaxed);
    return 0;
}
