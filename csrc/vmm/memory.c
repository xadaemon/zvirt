/*
 * Guest memory. Port of vmm/memory.zig.
 *
 * The range checks are written so they can't overflow. In Zig, `addr + size` overflowing panics;
 * in C it would wrap silently and let a guest-supplied address pass the check.
 */
#include "vmm/memory.h"

#include "test_utils/mmap.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The Zig version counts slots in a u8, which overflows when the 256th region is added. */
#define MAX_REGIONS 255

static bool region_contains(const struct zv_memory_region *region, uint64_t addr)
{
    return addr >= region->gpa && addr - region->gpa < region->size;
}

static bool region_contains_range(const struct zv_memory_region *region, uint64_t addr, size_t size)
{
    return size <= region->size && addr >= region->gpa && addr - region->gpa <= region->size - size;
}

/* Writes the part of `data` inside `region`, starting at `addr`. Returns the bytes written. */
static size_t region_write(struct zv_memory_region *region, uint64_t addr, const uint8_t *data,
                           size_t size)
{
    if (!region_contains(region, addr))
        return 0;

    size_t offset = addr - region->gpa;
    size_t room = region->size - offset;
    size_t can_write = size < room ? size : room;

    memcpy(region->raw + offset, data, can_write);
    return can_write;
}

int zv_guest_memory_new(struct zv_guest_memory **out)
{
    struct zv_guest_memory *memory = calloc(1, sizeof(*memory));

    if (memory == NULL)
        return -ENOMEM;

    *out = memory;
    return 0;
}

void zv_guest_memory_deinit(struct zv_guest_memory *memory)
{
    for (size_t i = 0; i < memory->region_count; i++) {
        struct zv_memory_region *region = &memory->regions[i];

        if (region->mmaped)
            zv_munmap(region->raw, region->size);
    }

    free(memory->regions);
    free(memory);
}

int zv_guest_memory_add(struct zv_guest_memory *memory, uint64_t gpa, void *raw, size_t size,
                        bool mmaped)
{
    if (memory->region_count == MAX_REGIONS) {
        fprintf(stderr, "guest memory: out of KVM memory slots\n");
        abort();
    }

    if (memory->region_count == memory->region_capacity) {
        size_t new_capacity = memory->region_capacity == 0 ? 8 : memory->region_capacity * 2;
        struct zv_memory_region *new_regions =
            realloc(memory->regions, new_capacity * sizeof(*new_regions));

        if (new_regions == NULL)
            return -ENOMEM;

        memory->regions = new_regions;
        memory->region_capacity = new_capacity;
    }

    struct zv_memory_region *region = &memory->regions[memory->region_count];

    region->raw = raw;
    region->size = size;
    region->gpa = gpa;
    region->slot = (uint8_t)memory->region_count;
    region->mmaped = mmaped;

    memory->region_count++;
    return 0;
}

int zv_guest_memory_as_slice(struct zv_guest_memory *memory, uint64_t gpa, size_t size,
                             uint8_t **out)
{
    for (size_t i = 0; i < memory->region_count; i++) {
        struct zv_memory_region *region = &memory->regions[i];

        if (region_contains_range(region, gpa, size)) {
            *out = region->raw + (gpa - region->gpa);
            return 0;
        }
    }

    return -EFAULT;
}

int zv_guest_memory_write(struct zv_guest_memory *memory, uint64_t gpa, const void *data,
                          size_t size)
{
    uint64_t current_gpa = gpa;
    const uint8_t *remaining = data;
    size_t remaining_size = size;

    /* Regions are visited in the order they were added, as in the Zig version. */
    for (size_t i = 0; i < memory->region_count; i++) {
        size_t written = region_write(&memory->regions[i], current_gpa, remaining, remaining_size);

        current_gpa += written;
        remaining += written;
        remaining_size -= written;
    }

    if (remaining_size != 0)
        return -EFAULT;

    return 0;
}
