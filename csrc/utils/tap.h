/*
 * TUN/TAP helper. Port of utils/tap.zig.
 *
 * Attaches to a TAP network interface (which must already exist and be usable by this user, see
 * `just tap`) and exchanges raw Ethernet frames with it. The fd is non-blocking.
 *
 * The read functions deliberately differ on "no packet available", as in the Zig version:
 * zv_tap_readv() returns 0, zv_tap_read() returns -EAGAIN.
 */
#ifndef ZV_UTILS_TAP_H
#define ZV_UTILS_TAP_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/uio.h>

struct zv_tap {
    int fd;
};

/* Opens the TAP interface `name`. Returns 0, -ENAMETOOLONG, or another -errno. */
int zv_tap_new(const char *name, struct zv_tap *out);

void zv_tap_deinit(struct zv_tap *tap);

/* Reads one frame into the buffers. Returns bytes read, 0 if no frame is waiting, or -errno. */
ssize_t zv_tap_readv(struct zv_tap *tap, const struct iovec *iovecs, int iovec_count);

/* Writes one frame from the buffers. Returns bytes written or -errno. */
ssize_t zv_tap_writev(struct zv_tap *tap, const struct iovec *iovecs, int iovec_count);

/* Reads one frame. Returns bytes read, -EAGAIN if no frame is waiting, or another -errno. */
ssize_t zv_tap_read(struct zv_tap *tap, void *data, size_t length);

/* Writes one frame. Returns bytes written, or -errno after logging an error. */
ssize_t zv_tap_write(struct zv_tap *tap, const void *data, size_t length);

#endif
