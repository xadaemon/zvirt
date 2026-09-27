/*
 * ID allocator. Port of utils/idalloc.zig.
 */
#include "utils/idalloc.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

_Noreturn static void idalloc_panic(const char *message)
{
    fprintf(stderr, "id allocator: %s\n", message);
    abort();
}

static uint64_t bit_for(size_t id)
{
    return (uint64_t)1 << id;
}

static bool is_used(const struct zv_id_allocator *allocator, size_t id)
{
    return (allocator->used & bit_for(id)) != 0;
}

void zv_id_allocator_init(struct zv_id_allocator *allocator, size_t count)
{
    if (count > ZV_ID_ALLOCATOR_MAX_IDS)
        idalloc_panic("too many ids requested");

    allocator->used = 0;
    allocator->count = count;
}

int zv_id_allocator_allocate(struct zv_id_allocator *allocator, size_t *id)
{
    for (size_t candidate = 0; candidate < allocator->count; candidate++) {
        if (!is_used(allocator, candidate)) {
            allocator->used |= bit_for(candidate);
            *id = candidate;
            return 0;
        }
    }

    return -ENOSPC;
}

int zv_id_allocator_allocate_specific(struct zv_id_allocator *allocator, size_t id)
{
    if (id >= allocator->count)
        idalloc_panic("out of range id requested");

    if (is_used(allocator, id))
        return -EBUSY;

    allocator->used |= bit_for(id);
    return 0;
}

void zv_id_allocator_free(struct zv_id_allocator *allocator, size_t id)
{
    if (id >= allocator->count)
        idalloc_panic("out of range id freed");

    if (!is_used(allocator, id))
        idalloc_panic("non-allocated id freed");

    allocator->used &= ~bit_for(id);
}
