/*
 * Tests for vmm/arch/x86/paging.c. New; paging.zig has no tests.
 */
#include "vmm/arch/x86/paging.h"

#include "test_runner.h"

static void test_identity_maps_first_gib(void)
{
    uint64_t pgd[ZV_X86_PAGE_TABLE_ENTRIES];
    uint64_t pud[ZV_X86_PAGE_TABLE_ENTRIES];
    uint64_t pmd[ZV_X86_PAGE_TABLE_ENTRIES];

    zv_x86_paging_fill_pgd(pgd);
    zv_x86_paging_fill_pud(pud);
    zv_x86_paging_fill_pmd(pmd);

    /* Each points to the next table: present | writable. */
    ZV_EXPECT(pgd[0] == 0x2003);
    ZV_EXPECT(pgd[1] == 0);
    ZV_EXPECT(pud[0] == 0x3003);
    ZV_EXPECT(pud[511] == 0);

    /* 2 MiB pages: present | writable | huge. */
    ZV_EXPECT(pmd[0] == 0x83);
    ZV_EXPECT(pmd[1] == ((uint64_t)1 << 21 | 0x83));
    ZV_EXPECT(pmd[511] == ((uint64_t)511 << 21 | 0x83));
}

static const struct zv_test tests[] = {
    {"identity maps the first GiB", test_identity_maps_first_gib},
};

const struct zv_test_suite zv_vmm_x86_paging_suite = {"vmm.arch.x86.paging", tests,
                                                      ZV_ARRAY_LEN(tests)};
