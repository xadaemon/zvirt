/*
 * Test executable for the kvm module. build.zig has no kvm test step (the Zig kvm code is only
 * exercised through the vmm tests), so these smoke tests have no Zig counterpart.
 *
 * Nothing here may open /dev/kvm: KVM objects must be created inside each forked test.
 */
#include "test_runner.h"

extern const struct zv_test_suite zv_kvm_suite;
extern const struct zv_test_suite zv_kvm_cpuid_suite;
extern const struct zv_test_suite zv_kvm_vcpu_suite;
extern const struct zv_test_suite zv_kvm_vm_suite;

int main(int argc, char **argv)
{
    const struct zv_test_suite *suites[] = {
        &zv_kvm_suite,
        &zv_kvm_cpuid_suite,
        &zv_kvm_vcpu_suite,
        &zv_kvm_vm_suite,
    };

    return zv_test_main(argc, argv, suites, ZV_ARRAY_LEN(suites));
}
