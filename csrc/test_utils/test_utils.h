/*
 * Helpers shared by integration tests. Port of test_utils/root.zig.
 *
 * Test-only helpers don't use the 0/-errno convention. They return a bool or a count, and abort
 * when the environment is broken (for example /proc/self/fd can't be opened), since no test can
 * continue from that.
 *
 * run_program() is ported in phase 8 with the network tests that use it.
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

/*
 * A temporary file that captures a guest console (TmpUartOutput in Zig). The VM writes to `fd`;
 * the test reads the whole file back with zv_tmp_uart_output_read().
 */
struct zv_tmp_uart_output {
    char directory[64];
    char path[96];
    int fd;
};

void zv_tmp_uart_output_create(struct zv_tmp_uart_output *output);

/* Closes the file and removes it and its directory. */
void zv_tmp_uart_output_deinit(struct zv_tmp_uart_output *output);

/*
 * Reads the file from the start into `buffer` and NUL-terminates it (so it can be searched with
 * string functions). Returns the number of bytes read, at most `size - 1`.
 */
size_t zv_tmp_uart_output_read(const struct zv_tmp_uart_output *output, char *buffer, size_t size);

#endif
