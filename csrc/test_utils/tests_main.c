/*
 * Test executable for the test_utils module (the `test_util_tests` step in build.zig).
 */
#include "test_runner.h"

extern const struct zv_test_suite zv_test_utils_suite;
extern const struct zv_test_suite zv_test_utils_mmap_suite;

int main(int argc, char **argv)
{
    const struct zv_test_suite *suites[] = {
        &zv_test_utils_suite,
        &zv_test_utils_mmap_suite,
    };

    return zv_test_main(argc, argv, suites, ZV_ARRAY_LEN(suites));
}
