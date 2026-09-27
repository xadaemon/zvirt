/*
 * Smoke tests for kvm/vcpu.c. New; the Zig kvm module has no tests of its own.
 */
#include "kvm/vcpu.h"

#include "kvm/kvm.h"
#include "test_runner.h"
#include "test_utils/mmap.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#define GUEST_CODE_ADDRESS 0x1000
#define GUEST_PAGE_SIZE 0x1000
#define COM1_PORT 0x3f8

/* An address with no RAM behind it, so guest accesses become MMIO exits. */
#define UNMAPPED_ADDRESS 0x8000

/* 16-bit real-mode code, assembled with `llvm-mc -triple=i386 --show-encoding`. */
static const uint8_t guest_code[] = {
    0xb0, 0x48,       /* mov $0x48, %al       ('H') */
    0xba, 0xf8, 0x03, /* mov $0x3f8, %dx */
    0xee,             /* out %al, %dx         -> I/O exit */
    0xa2, 0x00, 0x80, /* mov %al, (0x8000)    -> MMIO write exit */
    0xa0, 0x00, 0x80, /* mov (0x8000), %al    -> MMIO read exit */
    0xee,             /* out %al, %dx         -> I/O exit with the value read */
    0xf4,             /* hlt                  -> halt exit */
};

static const struct zv_kvm_io_result no_result = {.kind = ZV_KVM_IO_RESULT_NONE};

static void expect_out_exit(const struct zv_kvm_exit *exit_info, uint8_t value)
{
    ZV_EXPECT_EQ(ZV_KVM_EXIT_IO, exit_info->kind);
    ZV_EXPECT_EQ(ZV_KVM_IO_OUT, exit_info->io.direction);
    ZV_EXPECT_EQ(1, exit_info->io.size);
    ZV_EXPECT_EQ(COM1_PORT, exit_info->io.port);
    ZV_EXPECT_EQ(1, exit_info->io.count);
    ZV_EXPECT_EQ(value, exit_info->io.data[0]);
}

static void test_real_mode_guest_exits(void)
{
    struct zv_kvm kvm;
    struct zv_kvm_vm vm;

    ZV_EXPECT_EQ(0, zv_kvm_init(&kvm));
    ZV_EXPECT_EQ(0, zv_kvm_create_vm(&kvm, &vm));

    /*
     * No in-kernel irqchip on purpose: with one, the kernel handles HLT itself and, with
     * interrupts disabled, KVM_RUN would never return.
     */
    void *memory = NULL;

    ZV_EXPECT_EQ(0, zv_mmap(NULL, GUEST_PAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, &memory));
    memcpy(memory, guest_code, sizeof(guest_code));
    ZV_EXPECT_EQ(
        0, zv_kvm_vm_set_user_memory_region(&vm, GUEST_CODE_ADDRESS, 0, memory, GUEST_PAGE_SIZE));

    struct zv_kvm_vcpu vcpu;

    ZV_EXPECT_EQ(0, zv_kvm_vm_create_vcpu(&vm, 0, 1, &vcpu));

    /* The reset state runs from 0xffff0000; point the code segment at address 0 instead. */
    struct kvm_sregs sregs;

    ZV_EXPECT_EQ(0, zv_kvm_vcpu_get_sregs(&vcpu, &sregs));
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_set_sregs(&vcpu, &sregs));

    struct kvm_regs regs = {0};

    regs.rip = GUEST_CODE_ADDRESS;
    regs.rflags = 0x2; /* Bit 1 is reserved and must be set. */
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_set_regs(&vcpu, &regs));

    struct zv_kvm_exit exit_info;

    /* out %al, %dx */
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_run_once(&vcpu, no_result));
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_exit_reason(&vcpu, &exit_info));
    expect_out_exit(&exit_info, 'H');

    /* mov %al, (0x8000) */
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_run_once(&vcpu, no_result));
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_exit_reason(&vcpu, &exit_info));
    ZV_EXPECT_EQ(ZV_KVM_EXIT_MMIO, exit_info.kind);
    ZV_EXPECT_EQ(UNMAPPED_ADDRESS, exit_info.mmio.physical_address);
    ZV_EXPECT(exit_info.mmio.is_write);
    ZV_EXPECT_EQ(1, exit_info.mmio.length);
    ZV_EXPECT_EQ('H', exit_info.mmio.data & 0xff);

    /* mov (0x8000), %al */
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_run_once(&vcpu, no_result));
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_exit_reason(&vcpu, &exit_info));
    ZV_EXPECT_EQ(ZV_KVM_EXIT_MMIO, exit_info.kind);
    ZV_EXPECT_EQ(UNMAPPED_ADDRESS, exit_info.mmio.physical_address);
    ZV_EXPECT(!exit_info.mmio.is_write);
    ZV_EXPECT_EQ(1, exit_info.mmio.length);

    /* Answer the read with 0x5a; the guest then writes it to the port. */
    struct zv_kvm_io_result read_result = {.kind = ZV_KVM_IO_RESULT_MMIO, .mmio_data = 0x5a};

    ZV_EXPECT_EQ(0, zv_kvm_vcpu_run_once(&vcpu, read_result));
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_exit_reason(&vcpu, &exit_info));
    expect_out_exit(&exit_info, 0x5a);

    /* hlt */
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_run_once(&vcpu, no_result));
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_exit_reason(&vcpu, &exit_info));
    ZV_EXPECT_EQ(ZV_KVM_EXIT_HALT, exit_info.kind);

    zv_kvm_vcpu_deinit(&vcpu);
    zv_kvm_vm_deinit(&vm);
    zv_munmap(memory, GUEST_PAGE_SIZE);
    zv_kvm_deinit(&kvm);
}

static void test_immediate_exit_interrupts_run(void)
{
    struct zv_kvm kvm;
    struct zv_kvm_vm vm;
    struct zv_kvm_vcpu vcpu;

    ZV_EXPECT_EQ(0, zv_kvm_init(&kvm));
    ZV_EXPECT_EQ(0, zv_kvm_create_vm(&kvm, &vm));
    ZV_EXPECT_EQ(0, zv_kvm_vm_create_vcpu(&vm, 0, 1, &vcpu));

    /* KVM_RUN returns -EINTR without entering the guest. vm.stop() relies on this. */
    zv_kvm_vcpu_immediate_exit(&vcpu);
    ZV_EXPECT_EQ(-EINTR, zv_kvm_vcpu_run_once(&vcpu, no_result));

    zv_kvm_vcpu_deinit(&vcpu);
    zv_kvm_vm_deinit(&vm);
    zv_kvm_deinit(&kvm);
}

static const struct zv_test tests[] = {
    {"real-mode guest exits are decoded", test_real_mode_guest_exits},
    {"immediate exit interrupts KVM_RUN", test_immediate_exit_interrupts_run},
};

const struct zv_test_suite zv_kvm_vcpu_suite = {"kvm.vcpu", tests, ZV_ARRAY_LEN(tests)};
