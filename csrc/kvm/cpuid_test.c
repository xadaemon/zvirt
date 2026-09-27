/*
 * Smoke tests for kvm/cpuid.c. New; the Zig kvm module has no tests of its own.
 */
#include "kvm/cpuid.h"

#include "kvm/ioctl.h"
#include "kvm/kvm.h"
#include "test_runner.h"

#include <linux/kvm.h>

static void test_topology_leaves_are_patched(void)
{
    const uint32_t vcpu_id = 3;
    const uint32_t num_cpus = 16;

    struct zv_kvm kvm;
    struct zv_kvm_vm vm;
    struct zv_kvm_vcpu vcpu;

    ZV_EXPECT_EQ(0, zv_kvm_init(&kvm));
    ZV_EXPECT_EQ(0, zv_kvm_create_vm(&kvm, &vm));
    ZV_EXPECT_EQ(0, zv_kvm_vm_create_vcpu(&vm, vcpu_id, num_cpus, &vcpu));

    struct zv_kvm_cpuid_table cpuid = {0};

    cpuid.nent = ZV_KVM_CPUID_MAX_ENTRIES;
    ZV_EXPECT(zv_ioctl(vcpu.fd, KVM_GET_CPUID2, (unsigned long)&cpuid) >= 0);

    int feature_leaves = 0;

    for (uint32_t i = 0; i < cpuid.nent; i++) {
        const struct kvm_cpuid_entry2 *entry = &cpuid.entries[i];

        if (entry->function == 0x1) {
            feature_leaves++;
            ZV_EXPECT_EQ(num_cpus, (entry->ebx >> 16) & 0xff);
            ZV_EXPECT_EQ(vcpu_id, (entry->ebx >> 24) & 0xff);
        }

        /* Leaf 0xB is only present on some host CPUs; check it when it is. */
        if (entry->function == 0xB)
            ZV_EXPECT_EQ(vcpu_id, entry->edx);
    }

    ZV_EXPECT_EQ(1, feature_leaves);

    zv_kvm_vcpu_deinit(&vcpu);
    zv_kvm_vm_deinit(&vm);
    zv_kvm_deinit(&kvm);
}

static const struct zv_test tests[] = {
    {"topology leaves are patched", test_topology_leaves_are_patched},
};

const struct zv_test_suite zv_kvm_cpuid_suite = {"kvm.cpuid", tests, ZV_ARRAY_LEN(tests)};
