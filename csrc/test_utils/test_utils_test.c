/*
 * Tests for test_utils/test_utils.c. Port of the test blocks in test_utils/root.zig.
 */
#include "test_utils/test_utils.h"

#include "test_runner.h"

#include <fcntl.h>
#include <unistd.h>

static void test_leak_detector_works(void)
{
    /* Nothing opened in between: no leak. */
    struct zv_fd_leak_snapshot snapshot = zv_fd_leak_snapshot();

    ZV_EXPECT(!zv_fd_leak_check(&snapshot));

    /* One fd opened in between: a leak. Use the quiet check so the expected leak isn't logged. */
    snapshot = zv_fd_leak_snapshot();

    int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);

    ZV_EXPECT(fd >= 0);
    ZV_EXPECT(zv_fd_leak_check_ex(&snapshot, false));

    close(fd);
}

static const struct zv_test tests[] = {
    {"leak detector works", test_leak_detector_works},
};

const struct zv_test_suite zv_test_utils_suite = {"test_utils", tests, ZV_ARRAY_LEN(tests)};
