/*
 * Whole-file reading. New; replaces std.Io.Dir.readFileAlloc().
 */
#include "utils/file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Starting buffer size for files that report no size, like those in /proc. */
#define UNKNOWN_SIZE_CAPACITY 4096

int zv_read_file(const char *path, uint8_t **data, size_t *size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);

    if (fd < 0)
        return -errno;

    int err = 0;
    uint8_t *buffer = NULL;
    struct stat info;

    if (fstat(fd, &info) != 0) {
        err = -errno;
        goto fail;
    }

    /* One extra byte leaves room for the final read that reports end of file. */
    size_t capacity = info.st_size > 0 ? (size_t)info.st_size + 1 : UNKNOWN_SIZE_CAPACITY;
    size_t used = 0;

    buffer = malloc(capacity);
    if (buffer == NULL) {
        err = -ENOMEM;
        goto fail;
    }

    /* read() may return fewer bytes than asked for, so keep going until end of file. */
    while (true) {
        if (used == capacity) {
            uint8_t *bigger = realloc(buffer, capacity * 2);

            if (bigger == NULL) {
                err = -ENOMEM;
                goto fail;
            }

            buffer = bigger;
            capacity *= 2;
        }

        ssize_t bytes_read = read(fd, buffer + used, capacity - used);

        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read < 0) {
            err = -errno;
            goto fail;
        }
        if (bytes_read == 0)
            break;

        used += (size_t)bytes_read;
    }

    close(fd);
    *data = buffer;
    *size = used;
    return 0;

fail:
    free(buffer);
    close(fd);
    return err;
}
