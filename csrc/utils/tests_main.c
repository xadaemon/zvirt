/*
 * Test executable for the utils module (the `util_tests` step in build.zig).
 */
#include "test_runner.h"

extern const struct zv_test_suite zv_utils_epoll_suite;
extern const struct zv_test_suite zv_utils_eventfd_suite;
extern const struct zv_test_suite zv_utils_file_suite;
extern const struct zv_test_suite zv_utils_idalloc_suite;
extern const struct zv_test_suite zv_utils_mac_suite;
extern const struct zv_test_suite zv_utils_tap_suite;

int main(int argc, char **argv)
{
    const struct zv_test_suite *suites[] = {
        &zv_utils_epoll_suite,   &zv_utils_eventfd_suite, &zv_utils_file_suite,
        &zv_utils_idalloc_suite, &zv_utils_mac_suite,     &zv_utils_tap_suite,
    };

    return zv_test_main(argc, argv, suites, ZV_ARRAY_LEN(suites));
}
