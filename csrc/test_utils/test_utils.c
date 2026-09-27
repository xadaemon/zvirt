/*
 * Helpers shared by integration tests. Port of test_utils/root.zig.
 */
#include "test_utils/test_utils.h"

#include "utils/log.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_SCOPE "test_utils"

struct zv_fd_leak_snapshot zv_fd_leak_snapshot(void)
{
    DIR *dir = opendir("/proc/self/fd");

    if (dir == NULL) {
        perror("opendir /proc/self/fd");
        abort();
    }

    size_t count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        count++;
    }

    closedir(dir);

    /* The directory stream holds an fd of its own while we count. */
    return (struct zv_fd_leak_snapshot){.count = count - 1};
}

bool zv_fd_leak_check_ex(const struct zv_fd_leak_snapshot *snapshot, bool print)
{
    struct zv_fd_leak_snapshot now = zv_fd_leak_snapshot();

    if (now.count == snapshot->count)
        return false;

    if (print)
        zv_log_err(LOG_SCOPE, "FDLEAK: old %zu new %zu", snapshot->count, now.count);

    return true;
}

bool zv_fd_leak_check(const struct zv_fd_leak_snapshot *snapshot)
{
    return zv_fd_leak_check_ex(snapshot, true);
}
