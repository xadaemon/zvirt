/*
 * Configures a KVM vCPU's CPUID table. Port of kvm/cpuid.zig.
 */
#include "kvm/cpuid.h"

#include "kvm/ioctl.h"

#include <linux/kvm.h>

/* Leaf 0xB (extended topology): EDX holds the x2APIC id of the current logical CPU. */
#define CPUID_LEAF_TOPOLOGY 0xB

/*
 * Leaf 0x1 (feature information): EBX bits 16-23 hold the number of logical CPUs, bits 24-31 the
 * initial APIC id.
 */
#define CPUID_LEAF_FEATURES 0x1

int zv_kvm_cpuid_configure(int kvm_fd, uint32_t id, uint32_t num_cpus, int vcpu_fd)
{
    /* About 10 KiB, like the Zig version's stack variable. */
    struct zv_kvm_cpuid_table cpuid = {0};

    cpuid.nent = ZV_KVM_CPUID_MAX_ENTRIES;

    int rc = zv_ioctl(kvm_fd, KVM_GET_SUPPORTED_CPUID, (unsigned long)&cpuid);

    if (rc < 0)
        return rc;

    for (uint32_t i = 0; i < cpuid.nent; i++) {
        struct kvm_cpuid_entry2 *entry = &cpuid.entries[i];

        if (entry->function == CPUID_LEAF_TOPOLOGY) {
            entry->edx = id;
        } else if (entry->function == CPUID_LEAF_FEATURES) {
            uint32_t keep_low_bits = entry->ebx & 0x0000ffff;

            entry->ebx = keep_low_bits | (num_cpus << 16) | ((id & 0xff) << 24);
        }
    }

    return zv_ioctl_status(zv_ioctl(vcpu_fd, KVM_SET_CPUID2, (unsigned long)&cpuid));
}
