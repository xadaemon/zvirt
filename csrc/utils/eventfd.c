/*
 * eventfd helper. Port of utils/eventfd.zig.
 */
#include "utils/eventfd.h"

#include <assert.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int new_with_flags(uint32_t initial_value, int flags, struct zv_eventfd *out)
{
    int fd = eventfd(initial_value, flags);

    if (fd < 0)
        return -errno;

    out->fd = fd;
    return 0;
}

int zv_eventfd_new(uint32_t initial_value, struct zv_eventfd *out)
{
    return new_with_flags(initial_value, EFD_CLOEXEC | EFD_NONBLOCK, out);
}

int zv_eventfd_new_semaphore(uint32_t initial_value, struct zv_eventfd *out)
{
    return new_with_flags(initial_value, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE, out);
}

void zv_eventfd_deinit(struct zv_eventfd *eventfd)
{
    int rc = close(eventfd->fd);

    assert(rc == 0);
    (void)rc;
    eventfd->fd = -1;
}

int zv_eventfd_as_fd(const struct zv_eventfd *eventfd)
{
    return eventfd->fd;
}

int zv_eventfd_notify(const struct zv_eventfd *eventfd)
{
    return zv_eventfd_write(eventfd, 1);
}

int zv_eventfd_write(const struct zv_eventfd *eventfd, uint64_t value)
{
    ssize_t written = write(eventfd->fd, &value, sizeof(value));

    if (written < 0)
        return -errno;

    /* The kernel always transfers the whole 8-byte counter or fails. */
    assert(written == sizeof(value));
    return 0;
}

int zv_eventfd_read(const struct zv_eventfd *eventfd, uint64_t *value)
{
    ssize_t bytes_read = read(eventfd->fd, value, sizeof(*value));

    if (bytes_read < 0)
        return -errno;

    assert(bytes_read == sizeof(*value));
    return 0;
}
