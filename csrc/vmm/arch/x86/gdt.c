/*
 * x86 GDT helpers. Port of vmm/arch/x86/gdt.zig.
 */
#include "vmm/arch/x86/gdt.h"

/* Access byte: Present | S (code/data, not system) | RW (writable). */
#define GDT_DATA_ACCESS ((1 << 7) | (1 << 4) | (1 << 1))

/* Access byte: Present | S (code/data, not system) | E (executable). */
#define GDT_CODE_ACCESS ((1 << 7) | (1 << 4) | (1 << 3))

/* Flags nibble: G (limit counted in 4 KiB pages) | L (64-bit code). */
#define GDT_FLAGS ((1 << 3) | (1 << 1))

uint16_t zv_x86_gdt_selector(uint16_t index)
{
    /* Bits 0-1: requested privilege level 0. Bit 2: table indicator 0 (GDT). */
    return (uint16_t)(index << 3);
}

void zv_x86_gdt_setup_segment(struct kvm_segment *segment, bool code, uint16_t index)
{
    /* Unused in long mode. */
    segment->base = 0x0;
    /* Maximum. */
    segment->limit = 0xFFFFF;
    /* Long mode for the code segment. */
    segment->l = code ? 1 : 0;
    /* Limit counted in pages. */
    segment->g = 0x1;
    /* Type: execute/read/accessed for code, read/write/accessed for data. */
    segment->type = code ? 0xb : 0x3;
    segment->present = 0x1;
    /* Must be 0 in long mode. */
    segment->db = 0x0;
    segment->selector = zv_x86_gdt_selector(index);

    /* `s` and `dpl` are left as KVM reported them, as in the Zig version. */
}

/*
 * Packs one 8-byte descriptor. Field layout, from bit 0:
 * limit[0:16] base[16:40] access[40:48] limit_high[48:52] flags[52:56] base_high[56:64].
 * Base and limit are 0: long mode ignores them.
 */
static uint64_t gdt_entry(bool code)
{
    uint64_t limit_low = 0;
    uint64_t base_low = 0;
    uint64_t access = code ? GDT_CODE_ACCESS : GDT_DATA_ACCESS;
    uint64_t limit_high = 0;
    uint64_t flags = GDT_FLAGS;
    uint64_t base_high = 0;

    return limit_low | (base_low << 16) | (access << 40) | (limit_high << 48) | (flags << 52) |
           (base_high << 56);
}

struct zv_x86_gdt zv_x86_gdt_build(void)
{
    struct zv_x86_gdt gdt = {0};

    gdt.entries[0] = 0;
    gdt.entries[1] = 0;
    gdt.entries[ZV_X86_GDT_CODE_INDEX] = gdt_entry(true);
    gdt.entries[ZV_X86_GDT_DATA_INDEX] = gdt_entry(false);
    return gdt;
}
