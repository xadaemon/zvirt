/*
 * x86 GDT helpers. Port of vmm/arch/x86/gdt.zig.
 *
 * The boot vCPU starts directly in 64-bit long mode. It needs a GDT with a code and a data segment
 * at the indexes Linux expects, and matching cached segment registers.
 */
#ifndef ZV_VMM_ARCH_X86_GDT_H
#define ZV_VMM_ARCH_X86_GDT_H

#include <linux/kvm.h>
#include <stdbool.h>
#include <stdint.h>

/* As per Linux requirements (boot protocol: __BOOT_CS and __BOOT_DS). */
#define ZV_X86_GDT_CODE_INDEX 2
#define ZV_X86_GDT_DATA_INDEX 3

#define ZV_X86_GDT_ENTRY_COUNT 4

/* Four 8-byte descriptors: two null entries, the code segment and the data segment. */
struct zv_x86_gdt {
    uint64_t entries[ZV_X86_GDT_ENTRY_COUNT];
};

_Static_assert(sizeof(struct zv_x86_gdt) == 32, "the GDT is four 8-byte entries");

/* Segment selector for GDT entry `index`, with privilege level 0 and the GDT (not LDT) as table. */
uint16_t zv_x86_gdt_selector(uint16_t index);

/* Fills a KVM segment register to match the GDT entry at `index`. */
void zv_x86_gdt_setup_segment(struct kvm_segment *segment, bool code, uint16_t index);

struct zv_x86_gdt zv_x86_gdt_build(void);

#endif
