/*
 * ioctl wrapper. Port of kvm/ioctl.zig.
 */
#include "kvm/ioctl.h"

#include <errno.h>
#include <sys/ioctl.h>

int zv_ioctl(int fd, unsigned long request, unsigned long arg)
{
    int rc = ioctl(fd, request, arg);

    if (rc < 0)
        return -errno;

    return rc;
}

int zv_ioctl_status(int rc)
{
    return rc < 0 ? rc : 0;
}
