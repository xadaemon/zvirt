/*
 * Configures a KVM vCPU's CPUID table. Port of kvm/cpuid.zig.
 */
#ifndef ZV_KVM_CPUID_H
#define ZV_KVM_CPUID_H

#include <linux/kvm.h>
#include <stddef.h>
#include <stdint.h>

#define ZV_KVM_CPUID_MAX_ENTRIES 256

/*
 * struct kvm_cpuid2 with room for ZV_KVM_CPUID_MAX_ENTRIES entries. kvm_cpuid2 ends in a flexible
 * array, and embedding it in another struct isn't standard C, so this repeats its layout (as the
 * Zig version does) and checks that the offsets match.
 */
struct zv_kvm_cpuid_table {
    uint32_t nent;
    uint32_t padding;
    struct kvm_cpuid_entry2 entries[ZV_KVM_CPUID_MAX_ENTRIES];
};

_Static_assert(offsetof(struct zv_kvm_cpuid_table, nent) == offsetof(struct kvm_cpuid2, nent),
               "zv_kvm_cpuid_table.nent must match kvm_cpuid2");
_Static_assert(offsetof(struct zv_kvm_cpuid_table, entries) == offsetof(struct kvm_cpuid2, entries),
               "zv_kvm_cpuid_table.entries must match kvm_cpuid2");

/*
 * Gives vCPU `vcpu_fd` every CPUID leaf the host supports, patched so the guest sees `num_cpus`
 * logical CPUs and this vCPU's APIC id as `id`. Returns 0 or -errno.
 */
int zv_kvm_cpuid_configure(int kvm_fd, uint32_t id, uint32_t num_cpus, int vcpu_fd);

#endif
