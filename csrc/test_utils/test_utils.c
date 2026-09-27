/*
 * Helpers shared by integration tests. Port of test_utils/root.zig.
 */
#include "test_utils/test_utils.h"

#include "utils/log.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

void zv_tmp_uart_output_create(struct zv_tmp_uart_output *output)
{
    snprintf(output->directory, sizeof(output->directory), "/tmp/zvirt-test-XXXXXX");

    if (mkdtemp(output->directory) == NULL) {
        perror("mkdtemp");
        abort();
    }

    snprintf(output->path, sizeof(output->path), "%s/com1.out", output->directory);

    output->fd = open(output->path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (output->fd < 0) {
        perror("open console output file");
        abort();
    }
}

void zv_tmp_uart_output_deinit(struct zv_tmp_uart_output *output)
{
    close(output->fd);
    output->fd = -1;
    unlink(output->path);
    rmdir(output->directory);
}

size_t zv_tmp_uart_output_read(const struct zv_tmp_uart_output *output, char *buffer, size_t size)
{
    size_t total = 0;

    /* pread keeps the file offset where the VM is appending. */
    while (total < size - 1) {
        ssize_t bytes_read = pread(output->fd, buffer + total, size - 1 - total, (off_t)total);

        if (bytes_read < 0) {
            perror("read console output file");
            abort();
        }
        if (bytes_read == 0)
            break;

        total += (size_t)bytes_read;
    }

    buffer[total] = '\0';
    return total;
}
