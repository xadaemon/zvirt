/*
 * mmap wrapper with an optional leak tracker. Port of test_utils/mmap.zig.
 *
 * Differences from the Zig version:
 * - The hash map of ranges is a plain array with linear search. Tests hold a handful of mappings.
 * - The tracker doesn't log leaks itself. zv_mmap_tracker_stop() returns the count and the caller
 *   decides what to report.
 */
#include "test_utils/mmap.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

struct range {
    uintptr_t begin;
    size_t size;
};

/* All tracker state below is protected by `lock`, since vCPU threads map and unmap memory. */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool tracking = false;
static struct range *ranges = NULL;
static size_t range_count = 0;
static size_t range_capacity = 0;

_Noreturn static void tracker_panic(const char *message)
{
    fprintf(stderr, "mmap tracker: %s\n", message);
    abort();
}

/* Must be called with `lock` held. */
static void record_range(struct range range)
{
    if (range_count == range_capacity) {
        size_t new_capacity = range_capacity == 0 ? 16 : range_capacity * 2;
        struct range *new_ranges = realloc(ranges, new_capacity * sizeof(*ranges));

        if (new_ranges == NULL)
            tracker_panic("out of memory");

        ranges = new_ranges;
        range_capacity = new_capacity;
    }

    ranges[range_count] = range;
    range_count++;
}

/* Must be called with `lock` held. Returns false if the range was never recorded. */
static bool forget_range(struct range range)
{
    for (size_t i = 0; i < range_count; i++) {
        if (ranges[i].begin == range.begin && ranges[i].size == range.size) {
            /* Order doesn't matter, so move the last entry into the hole. */
            ranges[i] = ranges[range_count - 1];
            range_count--;
            return true;
        }
    }

    return false;
}

int zv_mmap(void *address, size_t length, int prot, int flags, int fd, off_t offset, void **out)
{
    void *mapping = mmap(address, length, prot, flags, fd, offset);

    if (mapping == MAP_FAILED)
        return -errno;

    pthread_mutex_lock(&lock);
    if (tracking)
        record_range((struct range){.begin = (uintptr_t)mapping, .size = length});
    pthread_mutex_unlock(&lock);

    *out = mapping;
    return 0;
}

void zv_munmap(void *address, size_t length)
{
    if (munmap(address, length) != 0)
        tracker_panic("munmap failed");

    pthread_mutex_lock(&lock);
    if (tracking && !forget_range((struct range){.begin = (uintptr_t)address, .size = length}))
        tracker_panic("invalid munmap: range wasn't mapped with zv_mmap()");
    pthread_mutex_unlock(&lock);
}

void zv_mmap_tracker_start(void)
{
    pthread_mutex_lock(&lock);
    if (tracking)
        tracker_panic("tracker already started");

    tracking = true;
    range_count = 0;
    pthread_mutex_unlock(&lock);
}

size_t zv_mmap_tracker_stop(void)
{
    pthread_mutex_lock(&lock);
    size_t leaked = range_count;

    tracking = false;
    free(ranges);
    ranges = NULL;
    range_count = 0;
    range_capacity = 0;
    pthread_mutex_unlock(&lock);

    return leaked;
}
