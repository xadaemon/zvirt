/*
 * Tests for utils/tap.c. Port of the test block in utils/tap.zig.
 *
 * The Zig test is commented out of utils/root.zig because it needs the `net0` TAP device from
 * `just tap`. Here it runs when net0 exists and is skipped otherwise.
 */
#include "utils/tap.h"

#include "test_runner.h"

#include <errno.h>
#include <unistd.h>

/* Keep in sync with the Justfile's `tap` recipe. */
#define TEST_IFACE "net0"

static void test_tap(void)
{
    /* Check before opening, so a missing device can't reach the error-logging path. */
    if (access("/sys/class/net/" TEST_IFACE, F_OK) != 0)
        zv_test_skip(TEST_IFACE " doesn't exist; create it with `just tap`");

    struct zv_tap tap;

    ZV_EXPECT_EQ(0, zv_tap_new(TEST_IFACE, &tap));
    zv_tap_deinit(&tap);
}

static void test_name_too_long(void)
{
    struct zv_tap tap;

    ZV_EXPECT_EQ(-ENAMETOOLONG, zv_tap_new("a-name-longer-than-ifnamsiz", &tap));
}

static const struct zv_test tests[] = {
    {"tap", test_tap},
    {"name too long", test_name_too_long},
};

const struct zv_test_suite zv_utils_tap_suite = {"utils.tap", tests, ZV_ARRAY_LEN(tests)};
