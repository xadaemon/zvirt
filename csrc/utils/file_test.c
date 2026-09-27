/*
 * Tests for utils/file.c. New, like the file itself.
 */
#include "utils/file.h"

#include "test_runner.h"

#include <errno.h>
#include <stdlib.h>

static void test_reads_whole_file(void)
{
    uint8_t *data = NULL;
    size_t size = 0;

    /* Tests run from the zvirt/ root. */
    ZV_EXPECT_EQ(0, zv_read_file("test_bins/64bit_guest.bin", &data, &size));
    ZV_EXPECT_EQ(29, size);
    free(data);

    /* /proc files report size 0 but have content. */
    ZV_EXPECT_EQ(0, zv_read_file("/proc/self/status", &data, &size));
    ZV_EXPECT(size > 0);
    free(data);
}

static void test_missing_file(void)
{
    uint8_t *data = NULL;
    size_t size = 0;

    ZV_EXPECT_EQ(-ENOENT, zv_read_file("test_bins/does-not-exist", &data, &size));
}

static const struct zv_test tests[] = {
    {"reads a whole file", test_reads_whole_file},
    {"missing file", test_missing_file},
};

const struct zv_test_suite zv_utils_file_suite = {"utils.file", tests, ZV_ARRAY_LEN(tests)};
