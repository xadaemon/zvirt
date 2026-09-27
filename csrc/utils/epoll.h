/*
 * epoll helper. Port of utils/epoll.zig.
 *
 * Each registered fd carries a 64-bit `context` that comes back in epoll_event.data.u64 when the fd
 * is ready. The VMM packs an event token into it to know which device the event belongs to.
 *
 * Note: glibc's struct epoll_event is packed on x86_64. Read its `data` field by value, never
 * through a pointer.
 */
#ifndef ZV_UTILS_EPOLL_H
#define ZV_UTILS_EPOLL_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>

struct zv_epoll {
    int fd;
};

/* Creates a close-on-exec epoll instance. Returns 0 or -errno. */
int zv_epoll_new(struct zv_epoll *out);

void zv_epoll_deinit(struct zv_epoll *epoll);

/* Watches `fd` for input (EPOLLIN). Returns 0 or -errno. */
int zv_epoll_add(struct zv_epoll *epoll, int fd, uint64_t context);

/* Watches `fd` for the given EPOLL* event bits. Returns 0 or -errno. */
int zv_epoll_add_with_events(struct zv_epoll *epoll, int fd, uint32_t events, uint64_t context);

/* Changes the events and context of an fd that is already watched. Returns 0 or -errno. */
int zv_epoll_modify(struct zv_epoll *epoll, int fd, uint32_t events, uint64_t context);

/* Stops watching `fd`. Returns 0 or -errno. */
int zv_epoll_remove(struct zv_epoll *epoll, int fd);

/*
 * Waits for events, with `mask` as the signal mask during the wait (NULL keeps the current one).
 * A timeout of -1 waits forever, 0 returns immediately.
 *
 * Returns the number of events stored in `events`, -EINVAL if `capacity` is 0, -EINTR if
 * interrupted by a signal, or another -errno.
 */
int zv_epoll_pwait(struct zv_epoll *epoll, struct epoll_event *events, size_t capacity,
                   int timeout_ms, const sigset_t *mask);

/* zv_epoll_pwait() without changing the signal mask. */
int zv_epoll_wait(struct zv_epoll *epoll, struct epoll_event *events, size_t capacity,
                  int timeout_ms);

#endif
