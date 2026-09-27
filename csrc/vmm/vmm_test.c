/*
 * Tests for vmm/vmm.c. Ported from the test blocks in vmm/root.zig (the virtio and network tests
 * follow in phases 6 to 8), plus one new test for the not-yet-ported devices.
 */
#include "vmm/vmm.h"

#include "test_runner.h"
#include "test_utils/mmap.h"
#include "test_utils/test_utils.h"
#include "utils/file.h"
#include "utils/log.h"
#include "vmm/vcpu.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GIB ((size_t)1 << 30)

/* Room for a whole Linux boot log. */
#define CONSOLE_BUFFER_SIZE (1 << 20)

#define OUTPUT_TIMEOUT_SECONDS 10

/* ---- Helpers ---- */

/* A file loaded into memory, freed with free_file(). */
struct file {
    uint8_t *data;
    size_t size;
};

static struct file load_file(const char *path)
{
    struct file file;

    ZV_EXPECT_EQ(0, zv_read_file(path, &file.data, &file.size));
    return file;
}

static void free_file(struct file *file)
{
    free(file->data);
}

/* Opens the KVM handle, as every Zig VM test does first (so it isn't counted as an fd leak). */
static void open_kvm_system(void)
{
    struct zv_kvm *kvm = NULL;

    ZV_EXPECT_EQ(0, zv_vmm_kvm_system(&kvm));
}

static struct zv_vm_config config_for(const struct file *binary, const struct file *initramfs,
                                      size_t ram_size)
{
    struct zv_vm_config config = zv_vm_config_default();

    config.ram_size = ram_size;
    config.binary = binary->data;
    config.binary_size = binary->size;

    if (initramfs != NULL) {
        config.initramfs = initramfs->data;
        config.initramfs_size = initramfs->size;
    }

    return config;
}

static void attach_output(struct zv_vm *vm, uint8_t index, int input_fd, int output_fd)
{
    struct zv_vm_console_config console = {
        .input_fd = input_fd,
        .output_fd = output_fd,
        .index = index,
        .configure_terminal = false,
    };

    ZV_EXPECT_EQ(0, zv_vm_attach_console(vm, &console));
}

/* zv_vm_run() on a separate thread, as the Zig tests do with std.Thread.spawn. */
struct vm_thread {
    pthread_t thread;
    struct zv_vm *vm;
    int result;
};

static void *vm_thread_main(void *arg)
{
    struct vm_thread *vm_thread = arg;

    vm_thread->result = zv_vm_run(vm_thread->vm);
    return NULL;
}

static void start_vm_thread(struct vm_thread *vm_thread, struct zv_vm *vm)
{
    vm_thread->vm = vm;
    vm_thread->result = -1;
    ZV_EXPECT_EQ(0, pthread_create(&vm_thread->thread, NULL, vm_thread_main, vm_thread));
}

static void join_vm_thread(struct vm_thread *vm_thread)
{
    pthread_join(vm_thread->thread, NULL);
    ZV_EXPECT_EQ(0, vm_thread->result);
}

static void wait_until_running(struct zv_vm *vm)
{
    /* Busy loop until the VM starts, as in the Zig tests. */
    while (zv_vm_get_state(vm) != ZV_VM_RUNNING) {
    }
}

/*
 * Checks why the boot vCPU stopped. The Zig tests only check that run() succeeds, which a guest
 * stopping for the wrong reason would also pass.
 */
static void expect_boot_vcpu_exit(struct zv_vm *vm, enum zv_vcpu_exit_reason expected)
{
    ZV_EXPECT_EQ(expected, zv_vcpu_get_exit_reason(vm->vcpus[0]));
}

static double seconds_now(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

/* Waits up to 10 seconds for `needle` to appear on the console, like wait_for_output in Zig. */
static void wait_for_output(const struct zv_tmp_uart_output *output, const char *needle)
{
    static char buffer[CONSOLE_BUFFER_SIZE];
    double deadline = seconds_now() + OUTPUT_TIMEOUT_SECONDS;
    size_t length = 0;

    while (seconds_now() < deadline) {
        length = zv_tmp_uart_output_read(output, buffer, sizeof(buffer));

        /* memmem, not strstr: the console output can contain NUL bytes. */
        if (memmem(buffer, length, needle, strlen(needle)) != NULL)
            return;

        usleep(10 * 1000);
    }

    fprintf(stderr, "guest console output:\n%.*s\n", (int)length, buffer);
    zv_test_fail_at(__FILE__, __LINE__, "timed out waiting for \"%s\"", needle);
}

static void write_input(int fd, const char *text)
{
    ZV_EXPECT_EQ((ssize_t)strlen(text), write(fd, text, strlen(text)));
}

/* Login discards all input after <enter>, so input is sent in lock-step with the prompts. */
static void login_in_initrd(const struct zv_tmp_uart_output *output, int input_fd)
{
    wait_for_output(output, "login");
    write_input(input_fd, "root\n");

    wait_for_output(output, "Password:");
    write_input(input_fd, "root\n");

    wait_for_output(output, "# ");
}

/* A pipe for guest console input: the VM reads pipe[0], the test writes pipe[1]. */
static void make_input_pipe(int pipe_fds[2])
{
    ZV_EXPECT_EQ(0, pipe2(pipe_fds, O_CLOEXEC));
}

/* ---- Tests ---- */

static volatile sig_atomic_t old_handler_calls = 0;

static void old_handler(int signal)
{
    (void)signal;
    old_handler_calls++;
}

static void test_vm_restores_sigaction(void)
{
    open_kvm_system();

    struct file binary = load_file("test_bins/64bit_guest.bin");

    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = old_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0; /* no SA_RESTART */
    sigaction(SIGUSR1, &action, NULL);

    struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
    struct zv_vm *vm = NULL;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    zv_vm_deinit(vm);

    pthread_kill(pthread_self(), SIGUSR1);
    ZV_EXPECT_EQ(1, old_handler_calls);

    free_file(&binary);
}

static void test_console_attach(void)
{
    open_kvm_system();

    {
        struct file binary = load_file("test_bins/64bit_guest.bin");
        struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
        struct zv_vm *vm = NULL;

        ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));

        struct zv_vm_console_config console = {
            .input_fd = -1,
            .output_fd = STDOUT_FILENO,
            .index = 0,
        };

        ZV_EXPECT_EQ(0, zv_vm_attach_console(vm, &console));
        ZV_EXPECT_EQ(-EEXIST, zv_vm_attach_console(vm, &console));

        zv_vm_deinit(vm);
        free_file(&binary);
    }

    {
        struct file binary = load_file("test_bins/64bit_loop.bin");
        struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
        struct zv_vm *vm = NULL;
        struct vm_thread vm_thread;

        ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
        start_vm_thread(&vm_thread, vm);
        wait_until_running(vm);

        struct zv_vm_console_config console = {
            .input_fd = -1,
            .output_fd = STDOUT_FILENO,
            .index = 0,
        };

        ZV_EXPECT_EQ(-EINVAL, zv_vm_attach_console(vm, &console));

        ZV_EXPECT_EQ(0, zv_vm_stop(vm));
        join_vm_thread(&vm_thread);

        zv_vm_deinit(vm);
        free_file(&binary);
    }
}

static void test_cannot_run_vm_two_times(void)
{
    open_kvm_system();

    struct file binary = load_file("test_bins/64bit_guest.bin");
    struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
    struct zv_vm *vm = NULL;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    ZV_EXPECT_EQ(0, zv_vm_run(vm));
    ZV_EXPECT_EQ(-EALREADY, zv_vm_run(vm));

    zv_vm_deinit(vm);
    free_file(&binary);
}

static void test_cannot_stop_vm_two_times(void)
{
    open_kvm_system();

    {
        struct file binary = load_file("test_bins/64bit_guest.bin");
        struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
        struct zv_vm *vm = NULL;

        ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
        ZV_EXPECT_EQ(-EINVAL, zv_vm_stop(vm));
        ZV_EXPECT_EQ(0, zv_vm_run(vm));
        ZV_EXPECT_EQ(-EINVAL, zv_vm_stop(vm));

        zv_vm_deinit(vm);
        free_file(&binary);
    }

    {
        struct file binary = load_file("test_bins/64bit_loop.bin");
        struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
        struct zv_vm *vm = NULL;
        struct vm_thread vm_thread;

        ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
        start_vm_thread(&vm_thread, vm);
        wait_until_running(vm);

        ZV_EXPECT_EQ(0, zv_vm_stop(vm));
        ZV_EXPECT_EQ(-EINVAL, zv_vm_stop(vm));

        join_vm_thread(&vm_thread);
        zv_vm_deinit(vm);
        free_file(&binary);
    }
}

static void test_guest_port_write_reaches_com1(void)
{
    open_kvm_system();
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct file binary = load_file("test_bins/64bit_guest.bin");
    struct zv_tmp_uart_output uart_output;

    zv_tmp_uart_output_create(&uart_output);

    struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
    struct zv_vm *vm = NULL;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    attach_output(vm, 0, -1, uart_output.fd);
    ZV_EXPECT_EQ(0, zv_vm_run(vm));

    char captured[16];
    size_t length = zv_tmp_uart_output_read(&uart_output, captured, sizeof(captured));

    ZV_EXPECT_EQ(1, length);
    ZV_EXPECT_EQ('H', captured[0]);
    expect_boot_vcpu_exit(vm, ZV_VCPU_EXIT_TEST_EXIT);

    zv_vm_deinit(vm);
    zv_tmp_uart_output_deinit(&uart_output);
    free_file(&binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

static void test_linux_reaches_shutdown(void)
{
    open_kvm_system();
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct file binary = load_file("test_bins/bzImage");
    struct zv_tmp_uart_output uart_output;

    zv_tmp_uart_output_create(&uart_output);

    /* No initramfs: the kernel panics, and with panic=-1 reboot=t it triple-faults (shutdown). */
    struct zv_vm_config config = config_for(&binary, NULL, GIB);
    struct zv_vm *vm = NULL;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    attach_output(vm, 0, -1, uart_output.fd);
    ZV_EXPECT_EQ(0, zv_vm_run(vm));
    expect_boot_vcpu_exit(vm, ZV_VCPU_EXIT_SHUTDOWN);

    zv_vm_deinit(vm);
    zv_tmp_uart_output_deinit(&uart_output);
    free_file(&binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

/* Boots Linux with the test initrd and `cmdline`, logs in, and reboots. */
static void login_and_reboot(const char *cmdline, enum zv_vcpu_exit_reason expected_exit)
{
    open_kvm_system();
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct file binary = load_file("test_bins/bzImage");
    struct file initrd = load_file("test_bins/initrd.img");
    struct zv_tmp_uart_output uart_output;
    int input[2];

    zv_tmp_uart_output_create(&uart_output);
    make_input_pipe(input);

    struct zv_vm_config config = config_for(&binary, &initrd, GIB);

    config.cmdline = cmdline;

    struct zv_vm *vm = NULL;
    struct vm_thread vm_thread;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    attach_output(vm, 0, input[0], uart_output.fd);
    start_vm_thread(&vm_thread, vm);

    login_in_initrd(&uart_output, input[1]);
    write_input(input[1], "reboot\n");

    join_vm_thread(&vm_thread);
    expect_boot_vcpu_exit(vm, expected_exit);
    zv_vm_deinit(vm);

    close(input[0]);
    close(input[1]);
    zv_tmp_uart_output_deinit(&uart_output);
    free_file(&initrd);
    free_file(&binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

static void test_linux_login_and_reboot(void)
{
    /* reboot=t in the default command line: the reboot is a triple fault, i.e. a shutdown. */
    login_and_reboot(NULL, ZV_VCPU_EXIT_SHUTDOWN);
}

static void test_vcpu_handles_unknown_exit_reason(void)
{
    /*
     * With panic=default the kernel reboots through the BIOS reset vector, which is unmapped. That
     * access ends in KVM_EXIT_INTERNAL_ERROR, which the VMM must handle gracefully.
     */
    login_and_reboot("panic=default pci=off", ZV_VCPU_EXIT_INTERNAL_ERROR);
}

static void test_linux_reaches_console(void)
{
    open_kvm_system();
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct file binary = load_file("test_bins/bzImage");
    struct file initrd = load_file("test_bins/initrd.img");
    struct zv_tmp_uart_output uart_output;

    zv_tmp_uart_output_create(&uart_output);

    struct zv_vm_config config = config_for(&binary, &initrd, GIB);
    struct zv_vm *vm = NULL;
    struct vm_thread vm_thread;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    attach_output(vm, 0, -1, uart_output.fd);
    start_vm_thread(&vm_thread, vm);

    wait_for_output(&uart_output, "login");

    ZV_EXPECT_EQ(0, zv_vm_stop(vm));
    join_vm_thread(&vm_thread);
    zv_vm_deinit(vm);

    zv_tmp_uart_output_deinit(&uart_output);
    free_file(&initrd);
    free_file(&binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

static void test_smp_works(void)
{
    open_kvm_system();
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct file binary = load_file("test_bins/bzImage");
    struct file initrd = load_file("test_bins/initrd.img");
    struct zv_tmp_uart_output uart_output;
    int input[2];

    zv_tmp_uart_output_create(&uart_output);
    make_input_pipe(input);

    struct zv_vm_config config = config_for(&binary, &initrd, GIB);

    config.cmdline = "panic=default pci=off";
    config.smp = 16;

    struct zv_vm *vm = NULL;
    struct vm_thread vm_thread;

    ZV_EXPECT_EQ(0, zv_vm_new(&config, &vm));
    attach_output(vm, 0, input[0], uart_output.fd);
    start_vm_thread(&vm_thread, vm);

    login_in_initrd(&uart_output, input[1]);

    write_input(input[1], "cat /proc/cpuinfo | grep processor | wc -l\n");
    wait_for_output(&uart_output, "16");

    /* Every CPU has its own APIC id, i.e. the CPUID patching worked. */
    write_input(input[1], "cat /proc/cpuinfo | grep 'initial apicid' | uniq | wc -l\n");
    wait_for_output(&uart_output, "16");

    ZV_EXPECT_EQ(0, zv_vm_stop(vm));
    join_vm_thread(&vm_thread);
    zv_vm_deinit(vm);

    close(input[0]);
    close(input[1]);
    zv_tmp_uart_output_deinit(&uart_output);
    free_file(&initrd);
    free_file(&binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

static void test_unported_devices_are_rejected(void)
{
    struct file binary = load_file("test_bins/64bit_guest.bin");
    struct zv_vm_config config = config_for(&binary, NULL, 0x20000);
    struct zv_vm *vm = NULL;

    config.pci = true;
    ZV_EXPECT_EQ(-ENOTSUP, zv_vm_new(&config, &vm));

    config = config_for(&binary, NULL, 0x20000);
    config.block_device.path = "disk.img";
    ZV_EXPECT_EQ(-ENOTSUP, zv_vm_new(&config, &vm));

    config = config_for(&binary, NULL, 0x20000);
    config.network.enabled = true;
    ZV_EXPECT_EQ(-ENOTSUP, zv_vm_new(&config, &vm));

    free_file(&binary);
}

static const struct zv_test tests[] = {
    {"Vm restores sigaction", test_vm_restores_sigaction},
    {"Console attach", test_console_attach},
    {"Cannot run vm two times", test_cannot_run_vm_two_times},
    {"Cannot stop vm two times", test_cannot_stop_vm_two_times},
    {"guest port write reaches COM1 UART", test_guest_port_write_reaches_com1},
    {"linux reaches shutdown", test_linux_reaches_shutdown},
    {"linux login and reboot", test_linux_login_and_reboot},
    {"vCPU handles unknown exit reason gracefully", test_vcpu_handles_unknown_exit_reason},
    {"linux reaches console", test_linux_reaches_console},
    {"SMP works", test_smp_works},
    {"unported devices are rejected", test_unported_devices_are_rejected},
};

const struct zv_test_suite zv_vmm_suite = {"vmm", tests, ZV_ARRAY_LEN(tests)};
