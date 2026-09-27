/*
 * Tests for vmm/device/cmos.c and mc146818rtc.c. New; the Zig versions have no tests.
 */
#include "vmm/device/cmos.h"

#include "test_runner.h"

#include <errno.h>

static uint8_t read_register(struct zv_cmos *cmos, uint8_t index)
{
    uint8_t value = 0;

    ZV_EXPECT_EQ(0, zv_cmos_write_reg(cmos, ZV_CMOS_INDEX, index));
    ZV_EXPECT_EQ(0, zv_cmos_read_reg(cmos, ZV_CMOS_DATA, &value));
    return value;
}

static void test_status_defaults(void)
{
    struct zv_cmos cmos;

    zv_cmos_init(&cmos);

    ZV_EXPECT_EQ(0x26, read_register(&cmos, ZV_RTC_STATUS_A));
    ZV_EXPECT_EQ(0x06, read_register(&cmos, ZV_RTC_STATUS_B));
    ZV_EXPECT_EQ(0x00, read_register(&cmos, ZV_RTC_STATUS_C));
}

static void test_register_access(void)
{
    struct zv_cmos cmos;
    uint8_t value = 0;

    zv_cmos_init(&cmos);

    /* Bit 7 of the index is the NMI-disable bit, not part of the register number. */
    ZV_EXPECT_EQ(0, zv_cmos_write_reg(&cmos, ZV_CMOS_INDEX, 0x80 | ZV_RTC_MINUTES));
    ZV_EXPECT_EQ(0, zv_cmos_write_reg(&cmos, ZV_CMOS_DATA, 42));
    ZV_EXPECT_EQ(42, read_register(&cmos, ZV_RTC_MINUTES));

    /* The index port itself reads as 0xff. */
    ZV_EXPECT_EQ(0, zv_cmos_read_reg(&cmos, ZV_CMOS_INDEX, &value));
    ZV_EXPECT_EQ(0xff, value);

    /* Writes to the shutdown status register are ignored; other unknown registers are errors. */
    ZV_EXPECT_EQ(0, zv_cmos_write_reg(&cmos, ZV_CMOS_INDEX, 0xf));
    ZV_EXPECT_EQ(0, zv_cmos_write_reg(&cmos, ZV_CMOS_DATA, 1));
    ZV_EXPECT_EQ(0, zv_cmos_write_reg(&cmos, ZV_CMOS_INDEX, 0x20));
    ZV_EXPECT_EQ(-EINVAL, zv_cmos_write_reg(&cmos, ZV_CMOS_DATA, 1));
}

static const struct zv_test tests[] = {
    {"status defaults", test_status_defaults},
    {"register access", test_register_access},
};

const struct zv_test_suite zv_vmm_cmos_suite = {"vmm.device.cmos", tests, ZV_ARRAY_LEN(tests)};
