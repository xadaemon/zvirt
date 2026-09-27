/*
 * ID allocator. Port of utils/idalloc.zig.
 *
 * Hands out small integer ids (e.g. legacy IRQ numbers) from 0 up to `count - 1`. The Zig version
 * is generic over the id count. Here the count is set at runtime, up to ZV_ID_ALLOCATOR_MAX_IDS,
 * and the used ids are bits in a single 64-bit mask.
 */
#ifndef ZV_UTILS_IDALLOC_H
#define ZV_UTILS_IDALLOC_H

#include <stddef.h>
#include <stdint.h>

#define ZV_ID_ALLOCATOR_MAX_IDS 64

struct zv_id_allocator {
    /* Bit N is set when id N is allocated. */
    uint64_t used;
    size_t count;
};

/* Starts with every id free. Aborts if `count` is larger than ZV_ID_ALLOCATOR_MAX_IDS. */
void zv_id_allocator_init(struct zv_id_allocator *allocator, size_t count);

/* Allocates the lowest free id into *id. Returns 0, or -ENOSPC if every id is taken. */
int zv_id_allocator_allocate(struct zv_id_allocator *allocator, size_t *id);

/* Allocates exactly `id`. Returns 0, or -EBUSY if it is already taken. Aborts if out of range. */
int zv_id_allocator_allocate_specific(struct zv_id_allocator *allocator, size_t id);

/* Frees `id`. Aborts if it is out of range or wasn't allocated. */
void zv_id_allocator_free(struct zv_id_allocator *allocator, size_t id);

#endif
