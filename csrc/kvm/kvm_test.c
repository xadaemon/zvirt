/*
 * Smoke tests for kvm/kvm.c. The Zig kvm module has no tests of its own (it is only exercised
 * through the vmm tests), so these are new.
 *
 * Every KVM object is created inside the test: a VM belongs to the process that created it, so a
 * forked test child can't use one inherited from the runner.
 */
#include "kvm/kvm.h"

#include "test_runner.h"

static void test_init_reports_vcpu_mmap_size(void)
{
    struct zv_kvm kvm;

    ZV_EXPECT_EQ(0, zv_kvm_init(&kvm));
    ZV_EXPECT(kvm.vcpu_mmap_size >= sizeof(struct kvm_run));

    zv_kvm_deinit(&kvm);
}

static const struct zv_test tests[] = {
    {"init reports the vCPU mmap size", test_init_reports_vcpu_mmap_size},
};

const struct zv_test_suite zv_kvm_suite = {"kvm", tests, ZV_ARRAY_LEN(tests)};
