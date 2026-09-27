/*
 * eventfd helper. Port of utils/eventfd.zig.
 *
 * An eventfd is a 64-bit counter behind a file descriptor. Writing adds to the counter, reading
 * returns it and resets it to zero (or, in semaphore mode, returns 1 and decrements it). The VMM
 * uses them to wake the main loop and to deliver interrupts through KVM.
 */
#ifndef ZV_UTILS_EVENTFD_H
#define ZV_UTILS_EVENTFD_H

#include <stdint.h>

struct zv_eventfd {
    int fd;
};

/* Creates a non-blocking, close-on-exec eventfd counter. Returns 0 or -errno. */
int zv_eventfd_new(uint32_t initial_value, struct zv_eventfd *out);

/* Same as zv_eventfd_new(), but each read returns 1 and decrements the counter by one. */
int zv_eventfd_new_semaphore(uint32_t initial_value, struct zv_eventfd *out);

void zv_eventfd_deinit(struct zv_eventfd *eventfd);

int zv_eventfd_as_fd(const struct zv_eventfd *eventfd);

/* Adds 1 to the counter. Same return values as zv_eventfd_write(). */
int zv_eventfd_notify(const struct zv_eventfd *eventfd);

/*
 * Adds `value` to the counter. Returns 0, -EAGAIN if the counter would overflow, -EINTR if
 * interrupted by a signal, or another -errno.
 */
int zv_eventfd_write(const struct zv_eventfd *eventfd, uint64_t value);

/*
 * Reads the counter into *value. Returns 0, -EAGAIN if the counter is zero, -EINTR if interrupted
 * by a signal, or another -errno.
 */
int zv_eventfd_read(const struct zv_eventfd *eventfd, uint64_t *value);

#endif
