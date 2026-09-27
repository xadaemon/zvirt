/*
 * x86 ACPI setup. Port of vmm/arch/x86/acpi.zig.
 */
#ifndef ZV_VMM_ARCH_X86_ACPI_H
#define ZV_VMM_ARCH_X86_ACPI_H

#include "vmm/memory.h"

#include <stddef.h>

/*
 * Writes the RSDP, MADT and XSDT for `vcpu_count` CPUs into the guest's ACPI area
 * (ZV_X86_BOOT_ACPI_ADDR). The RSDP goes first, at the address the boot parameters point to.
 *
 * Returns 0, -EFAULT if the ACPI area isn't in guest memory, or -ENOMEM if the tables don't fit.
 *
 * The Zig version takes the whole Vm but only reads its memory and vCPU count, so those are the
 * parameters here.
 */
int zv_x86_acpi_setup_tables(struct zv_guest_memory *memory, size_t vcpu_count);

#endif
