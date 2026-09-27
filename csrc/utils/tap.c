/*
 * TUN/TAP helper. Port of utils/tap.zig.
 */
#include "utils/tap.h"

#include "utils/log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* The kernel's own definitions. Never mix these with <net/if.h>, the two headers conflict. */
#include <linux/if.h>
#include <linux/if_tun.h>

#define LOG_SCOPE "tap"

int zv_tap_new(const char *name, struct zv_tap *out)
{
    if (strlen(name) > IFNAMSIZ - 1)
        return -ENAMETOOLONG;

    int fd = open("/dev/net/tun", O_RDWR | O_CLOEXEC);

    if (fd < 0)
        return -errno;

    int err;
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0) {
        err = -errno;
        zv_log_err(LOG_SCOPE, "Failed to get file flags: %s", strerror(-err));
        goto close_fd;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        err = -errno;
        zv_log_err(LOG_SCOPE, "Failed to set file flags: %s", strerror(-err));
        goto close_fd;
    }

    struct ifreq request = {0};

    strncpy(request.ifr_name, name, IFNAMSIZ - 1);

    /* A TAP device carries Ethernet frames. IFF_NO_PI: no extra packet-info header per frame. */
    request.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (ioctl(fd, TUNSETIFF, &request) < 0) {
        err = -errno;
        zv_log_err(LOG_SCOPE, "Failed to TUNSETIFF: %s", strerror(-err));
        goto close_fd;
    }

    out->fd = fd;
    return 0;

close_fd:
    close(fd);
    return err;
}

void zv_tap_deinit(struct zv_tap *tap)
{
    /* The result is ignored on purpose, as in tap.zig (unlike the other deinit functions). */
    close(tap->fd);
    tap->fd = -1;
}

ssize_t zv_tap_readv(struct zv_tap *tap, const struct iovec *iovecs, int iovec_count)
{
    ssize_t bytes_read = readv(tap->fd, iovecs, iovec_count);

    if (bytes_read < 0 && errno == EAGAIN)
        return 0;
    if (bytes_read < 0)
        return -errno;

    return bytes_read;
}

ssize_t zv_tap_writev(struct zv_tap *tap, const struct iovec *iovecs, int iovec_count)
{
    ssize_t written = writev(tap->fd, iovecs, iovec_count);

    if (written < 0)
        return -errno;

    return written;
}

ssize_t zv_tap_read(struct zv_tap *tap, void *data, size_t length)
{
    ssize_t bytes_read = read(tap->fd, data, length);

    if (bytes_read < 0)
        return -errno;

    return bytes_read;
}

ssize_t zv_tap_write(struct zv_tap *tap, const void *data, size_t length)
{
    ssize_t written = write(tap->fd, data, length);

    if (written < 0) {
        int err = -errno;

        zv_log_err(LOG_SCOPE, "Failed to send packet: %s", strerror(-err));
        return err;
    }

    return written;
}
