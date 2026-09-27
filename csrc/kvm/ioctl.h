/*
 * ioctl wrapper. Port of kvm/ioctl.zig.
 */
#ifndef ZV_KVM_IOCTL_H
#define ZV_KVM_IOCTL_H

/*
 * Calls ioctl(fd, request, arg). Pointer arguments are passed as (unsigned long)&value.
 *
 * Returns the ioctl's non-negative result, or -errno. -EINTR (interrupted by a signal) and -EAGAIN
 * (try again) are passed through as-is; the caller decides whether to retry.
 */
int zv_ioctl(int fd, unsigned long request, unsigned long arg);

/*
 * For ioctls that return 0 on success: turns a zv_ioctl() result into 0 or -errno, so callers
 * don't pass a positive value on by mistake.
 */
int zv_ioctl_status(int rc);

#endif
