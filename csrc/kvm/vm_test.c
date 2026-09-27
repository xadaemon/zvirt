/*
 * Smoke tests for kvm/vm.c. New; the Zig kvm module has no tests of its own.
 */
#include "kvm/vm.h"

#include "kvm/kvm.h"
#include "test_runner.h"
#include "test_utils/mmap.h"
#include "test_utils/test_utils.h"
#include "utils/eventfd.h"

#include <errno.h>

static void test_setup_and_teardown_leak_nothing(void)
{
    struct zv_kvm kvm;

    ZV_EXPECT_EQ(0, zv_kvm_init(&kvm));

    /* Snapshot after opening /dev/kvm, which the VMM keeps open for the whole process. */
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    struct zv_kvm_vm vm;

    ZV_EXPECT_EQ(0, zv_kvm_create_vm(&kvm, &vm));

    /* The PIT needs the in-kernel irqchip, so the order matters. */
    ZV_EXPECT_EQ(0, zv_kvm_vm_create_irqchip(&vm));
    ZV_EXPECT_EQ(0, zv_kvm_vm_create_pit(&vm));

    struct zv_eventfd irq_event;
    struct zv_eventfd io_event;

    ZV_EXPECT_EQ(0, zv_eventfd_new(0, &irq_event));
    ZV_EXPECT_EQ(0, zv_eventfd_new(0, &io_event));
    ZV_EXPECT_EQ(0, zv_kvm_vm_register_irq(&vm, &irq_event, 5));
    ZV_EXPECT_EQ(0, zv_kvm_vm_register_ioevent(&vm, &io_event, 0xd0000000, 4, 1));
    ZV_EXPECT_EQ(0, zv_kvm_vm_irq_set(&vm, 5, true));
    ZV_EXPECT_EQ(0, zv_kvm_vm_irq_set(&vm, 5, false));

    struct zv_kvm_vcpu vcpu;

    ZV_EXPECT_EQ(0, zv_kvm_vm_create_vcpu(&vm, 0, 1, &vcpu));

    /* Special registers survive a get/set round trip. */
    struct kvm_sregs2 sregs;

    ZV_EXPECT_EQ(0, zv_kvm_vcpu_get_sregs2(&vcpu, &sregs));
    sregs.gdt.base = 0x500;
    sregs.gdt.limit = 31;
    ZV_EXPECT_EQ(0, zv_kvm_vcpu_set_sregs2(&vcpu, &sregs));

    struct kvm_sregs2 read_back;

    ZV_EXPECT_EQ(0, zv_kvm_vcpu_get_sregs2(&vcpu, &read_back));
    ZV_EXPECT_EQ(0x500, read_back.gdt.base);
    ZV_EXPECT_EQ(31, read_back.gdt.limit);

    /* KVM rejects a duplicate id before creating an fd, so there is nothing to clean up. */
    struct zv_kvm_vcpu duplicate;

    ZV_EXPECT_EQ(-EEXIST, zv_kvm_vm_create_vcpu(&vm, 0, 1, &duplicate));

    zv_kvm_vcpu_deinit(&vcpu);
    zv_eventfd_deinit(&io_event);
    zv_eventfd_deinit(&irq_event);
    zv_kvm_vm_deinit(&vm);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));

    zv_kvm_deinit(&kvm);
}

static const struct zv_test tests[] = {
    {"setup and teardown leak nothing", test_setup_and_teardown_leak_nothing},
};

const struct zv_test_suite zv_kvm_vm_suite = {"kvm.vm", tests, ZV_ARRAY_LEN(tests)};
