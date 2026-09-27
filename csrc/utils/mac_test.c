/*
 * Tests for utils/mac.c. Port of the test block in utils/mac.zig, plus a few malformed inputs.
 */
#include "utils/mac.h"

#include "test_runner.h"

#include <errno.h>
#include <string.h>

static void test_parsing(void)
{
    struct zv_mac mac;
    const uint8_t expected[ZV_MAC_LEN] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};

    ZV_EXPECT_EQ(0, zv_mac_from_str("11:11:11:11:11:11", &mac));
    ZV_EXPECT(memcmp(mac.bytes, expected, ZV_MAC_LEN) == 0);

    /* Too few octets, too many octets, wrong separator. */
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("11:11:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("11,11,11,11,11", &mac));
}

static void test_mixed_case_and_malformed_parts(void)
{
    struct zv_mac mac;
    const uint8_t expected[ZV_MAC_LEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x0f};

    ZV_EXPECT_EQ(0, zv_mac_from_str("aa:BB:cC:dd:EE:0f", &mac));
    ZV_EXPECT(memcmp(mac.bytes, expected, ZV_MAC_LEN) == 0);

    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("1:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("111:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str(" f:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("0x:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("gg:11:11:11:11:11", &mac));
    ZV_EXPECT_EQ(-EINVAL, zv_mac_from_str("11:11:11:11:11:11:", &mac));
}

static const struct zv_test tests[] = {
    {"Parsing", test_parsing},
    {"mixed case and malformed parts", test_mixed_case_and_malformed_parts},
};

const struct zv_test_suite zv_utils_mac_suite = {"utils.mac", tests, ZV_ARRAY_LEN(tests)};
