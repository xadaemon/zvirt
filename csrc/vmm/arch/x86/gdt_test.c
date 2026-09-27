/*
 * Tests for vmm/arch/x86/gdt.c. New; gdt.zig only has compile-time size checks. These pin the
 * descriptor bit layout, which Zig expressed as a packed struct and C builds with shifts.
 */
#include "vmm/arch/x86/gdt.h"

#include "test_runner.h"

static void test_descriptors(void)
{
    struct zv_x86_gdt gdt = zv_x86_gdt_build();

    ZV_EXPECT(gdt.entries[0] == 0);
    ZV_EXPECT(gdt.entries[1] == 0);

    /* Access 0x98 (present, code, executable), flags 0xA (4 KiB granularity, 64-bit). */
    ZV_EXPECT(gdt.entries[ZV_X86_GDT_CODE_INDEX] == 0x00A0980000000000);

    /* Access 0x92 (present, data, writable), same flags. */
    ZV_EXPECT(gdt.entries[ZV_X86_GDT_DATA_INDEX] == 0x00A0920000000000);
}

static void test_segments(void)
{
    struct kvm_segment code = {0};
    struct kvm_segment data = {0};

    zv_x86_gdt_setup_segment(&code, true, ZV_X86_GDT_CODE_INDEX);
    zv_x86_gdt_setup_segment(&data, false, ZV_X86_GDT_DATA_INDEX);

    ZV_EXPECT_EQ(0x10, code.selector);
    ZV_EXPECT_EQ(1, code.l);
    ZV_EXPECT_EQ(0xb, code.type);
    ZV_EXPECT_EQ(0xFFFFF, code.limit);

    ZV_EXPECT_EQ(0x18, data.selector);
    ZV_EXPECT_EQ(0, data.l);
    ZV_EXPECT_EQ(0x3, data.type);
}

static const struct zv_test tests[] = {
    {"descriptors", test_descriptors},
    {"segments", test_segments},
};

const struct zv_test_suite zv_vmm_x86_gdt_suite = {"vmm.arch.x86.gdt", tests, ZV_ARRAY_LEN(tests)};
