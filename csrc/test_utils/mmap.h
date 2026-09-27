/*
 * mmap wrapper with an optional leak tracker. Port of test_utils/mmap.zig.
 *
 * Production code (guest memory, the vCPU run area) maps memory through zv_mmap() and zv_munmap().
 * Normally they only forward to mmap()/munmap(). A test can call zv_mmap_tracker_start() to record
 * every mapping, then zv_mmap_tracker_stop() to find out how many were never unmapped.
 * AddressSanitizer doesn't track mmap, so this check fills that gap.
 *
 * Only mappings made through these wrappers are tracked. Allocator arenas are not, which is also
 * why the tracker doesn't simply compare the process's total mapped size.
 */
#ifndef ZV_TEST_UTILS_MMAP_H
#define ZV_TEST_UTILS_MMAP_H

#include <stddef.h>
#include <sys/types.h>

/*
 * Same arguments as mmap(2). On success stores the mapping in *out and returns 0. On failure
 * returns -errno.
 */
int zv_mmap(void *address, size_t length, int prot, int flags, int fd, off_t offset, void **out);

/*
 * Unmaps a mapping created by zv_mmap(). Aborts if munmap() fails, or if tracking is on and the
 * range was never recorded (a sign the caller bypassed the wrapper).
 */
void zv_munmap(void *address, size_t length);

/* Starts recording mappings. Aborts if tracking is already on. */
void zv_mmap_tracker_start(void);

/* Stops recording and returns the number of mappings that were never unmapped. */
size_t zv_mmap_tracker_stop(void);

#endif
