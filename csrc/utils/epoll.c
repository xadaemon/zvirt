/*
 * epoll helper. Port of utils/epoll.zig.
 */
#include "utils/epoll.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

int zv_epoll_new(struct zv_epoll *out)
{
    int fd = epoll_create1(EPOLL_CLOEXEC);

    if (fd < 0)
        return -errno;

    out->fd = fd;
    return 0;
}

void zv_epoll_deinit(struct zv_epoll *epoll)
{
    int rc = close(epoll->fd);

    assert(rc == 0);
    (void)rc;
    epoll->fd = -1;
}

static int control(struct zv_epoll *epoll, int operation, int fd, uint32_t events, uint64_t context)
{
    struct epoll_event event = {0};

    event.events = events;
    event.data.u64 = context;

    if (epoll_ctl(epoll->fd, operation, fd, &event) != 0)
        return -errno;

    return 0;
}

int zv_epoll_add(struct zv_epoll *epoll, int fd, uint64_t context)
{
    return zv_epoll_add_with_events(epoll, fd, EPOLLIN, context);
}

int zv_epoll_add_with_events(struct zv_epoll *epoll, int fd, uint32_t events, uint64_t context)
{
    return control(epoll, EPOLL_CTL_ADD, fd, events, context);
}

int zv_epoll_modify(struct zv_epoll *epoll, int fd, uint32_t events, uint64_t context)
{
    return control(epoll, EPOLL_CTL_MOD, fd, events, context);
}

int zv_epoll_remove(struct zv_epoll *epoll, int fd)
{
    if (epoll_ctl(epoll->fd, EPOLL_CTL_DEL, fd, NULL) != 0)
        return -errno;

    return 0;
}

int zv_epoll_pwait(struct zv_epoll *epoll, struct epoll_event *events, size_t capacity,
                   int timeout_ms, const sigset_t *mask)
{
    if (capacity == 0)
        return -EINVAL;

    /* The syscall takes the buffer size as an int. */
    int max_events = capacity > INT_MAX ? INT_MAX : (int)capacity;
    int count = epoll_pwait(epoll->fd, events, max_events, timeout_ms, mask);

    if (count < 0)
        return -errno;

    return count;
}

int zv_epoll_wait(struct zv_epoll *epoll, struct epoll_event *events, size_t capacity,
                  int timeout_ms)
{
    return zv_epoll_pwait(epoll, events, capacity, timeout_ms, NULL);
}
