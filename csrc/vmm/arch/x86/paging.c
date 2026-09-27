/*
 * Paging helpers. Port of vmm/arch/x86/paging.zig.
 */
#include "vmm/arch/x86/paging.h"

#include "vmm/arch/x86/layout.h"

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
/* In a PMD entry: maps a 2 MiB page directly instead of pointing to a page table. */
#define PAGE_HUGE (1 << 7)

#define HUGE_PAGE_SHIFT 21

static void clear(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES])
{
    for (int i = 0; i < ZV_X86_PAGE_TABLE_ENTRIES; i++)
        table[i] = 0;
}

void zv_x86_paging_fill_pgd(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES])
{
    clear(table);
    table[0] = ZV_X86_PUD_ADDR | PAGE_PRESENT | PAGE_WRITABLE;
}

void zv_x86_paging_fill_pud(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES])
{
    clear(table);
    table[0] = ZV_X86_PMD_ADDR | PAGE_PRESENT | PAGE_WRITABLE;
}

void zv_x86_paging_fill_pmd(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES])
{
    for (uint64_t index = 0; index < ZV_X86_PAGE_TABLE_ENTRIES; index++)
        table[index] = (index << HUGE_PAGE_SHIFT) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
}
