/*
 * Helpers shared by integration tests. Port of test_utils/root.zig.
 *
 * Test-only helpers don't use the 0/-errno convention. They return a bool or a count, and abort
 * when the environment is broken (for example /proc/self/fd can't be opened), since no test can
 * continue from that.
 *
 * run_program() and TmpUartOutput are ported in phase 5 with the VM tests that use them.
 */
#ifndef ZV_TEST_UTILS_TEST_UTILS_H
#define ZV_TEST_UTILS_TEST_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* Number of open file descriptors at the time of the snapshot. */
struct zv_fd_leak_snapshot {
    size_t count;
};

struct zv_fd_leak_snapshot zv_fd_leak_snapshot(void);

/* Returns true if the number of open fds differs from the snapshot, and logs an error if so. */
bool zv_fd_leak_check(const struct zv_fd_leak_snapshot *snapshot);

/*
 * Same as zv_fd_leak_check(), but only logs when `print` is true. A logged error fails the current
 * test, so tests that expect a leak must pass false.
 */
bool zv_fd_leak_check_ex(const struct zv_fd_leak_snapshot *snapshot, bool print);

#endif
