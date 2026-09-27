/*
 * Guest memory. Port of vmm/memory.zig.
 *
 * Guest RAM is a list of regions: host buffers mapped at guest physical addresses (gpa). All
 * access to guest memory from the VMM goes through zv_guest_memory_as_slice() or
 * zv_guest_memory_write(), which check that the whole range lies inside one region. Addresses
 * and sizes often come from the guest (e.g. virtio descriptors), so those checks are the VMM's
 * main defence against a guest pointing it outside its RAM.
 */
#ifndef ZV_VMM_MEMORY_H
#define ZV_VMM_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct zv_memory_region {
    /* Host memory backing the region. */
    uint8_t *raw;
    size_t size;
    /* Guest physical address of raw[0]. */
    uint64_t gpa;
    /* KVM memory slot number. */
    uint8_t slot;
    /* true if `raw` came from zv_mmap() and is unmapped on deinit. */
    bool mmaped;
};

struct zv_guest_memory {
    struct zv_memory_region *regions;
    size_t region_count;
    size_t region_capacity;
};

/* Allocates an empty guest memory. Returns 0 or -ENOMEM. */
int zv_guest_memory_new(struct zv_guest_memory **out);

/* Unmaps the mmaped regions and frees `memory`. */
void zv_guest_memory_deinit(struct zv_guest_memory *memory);

/*
 * Adds a region and gives it the next KVM slot number. Returns 0 or -ENOMEM. Aborts when all 255
 * slot numbers are used, like the Zig version's u8 counter overflowing.
 */
int zv_guest_memory_add(struct zv_guest_memory *memory, uint64_t gpa, void *raw, size_t size,
                        bool mmaped);

/*
 * Finds the host memory behind guest range [gpa, gpa + size). Returns 0 and stores a pointer in
 * *out, or -EFAULT if the range isn't entirely inside one region.
 */
int zv_guest_memory_as_slice(struct zv_guest_memory *memory, uint64_t gpa, size_t size,
                             uint8_t **out);

/*
 * Copies `data` into guest memory at `gpa`. The range may span consecutive regions. Returns 0, or
 * -EFAULT if some of it lies outside guest memory (the part that fits is still written).
 */
int zv_guest_memory_write(struct zv_guest_memory *memory, uint64_t gpa, const void *data,
                          size_t size);

#endif
