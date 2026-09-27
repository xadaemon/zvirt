/*
 * Paging helpers. Port of vmm/arch/x86/paging.zig.
 *
 * Long mode needs paging. For simplicity the boot page tables identity-map the first 1 GiB with
 * 2 MiB pages (1 GiB pages would need the PDPE1GB CPU feature):
 * PGD[0] -> PUD, PUD[0] -> PMD, PMD[i] -> the 2 MiB page at i * 2 MiB.
 *
 * The Zig version builds these tables at compile time; here each is filled by a function.
 */
#ifndef ZV_VMM_ARCH_X86_PAGING_H
#define ZV_VMM_ARCH_X86_PAGING_H

#include <stdint.h>

#define ZV_X86_PAGE_TABLE_ENTRIES 512

void zv_x86_paging_fill_pgd(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES]);
void zv_x86_paging_fill_pud(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES]);
void zv_x86_paging_fill_pmd(uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES]);

#endif
