/*
 * Tests for vmm/memory.c. New; vmm/memory.zig has no tests.
 */
#include "vmm/memory.h"

#include "test_runner.h"
#include "test_utils/mmap.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#define REGION_SIZE 0x1000

static void test_as_slice_checks_the_whole_range(void)
{
    uint8_t ram_a[REGION_SIZE];
    uint8_t ram_b[REGION_SIZE];
    struct zv_guest_memory *memory = NULL;
    uint8_t *slice = NULL;

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x0, ram_a, REGION_SIZE, false));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x1000, ram_b, REGION_SIZE, false));

    ZV_EXPECT_EQ(0, zv_guest_memory_as_slice(memory, 0x10, 0x20, &slice));
    ZV_EXPECT(slice == ram_a + 0x10);

    ZV_EXPECT_EQ(0, zv_guest_memory_as_slice(memory, 0x1000, REGION_SIZE, &slice));
    ZV_EXPECT(slice == ram_b);

    /* A slice must lie inside one region, even if the next region is contiguous. */
    ZV_EXPECT_EQ(-EFAULT, zv_guest_memory_as_slice(memory, 0xff0, 0x20, &slice));

    /* Past the end of guest memory. */
    ZV_EXPECT_EQ(-EFAULT, zv_guest_memory_as_slice(memory, 0x2000, 1, &slice));

    /* gpa + size overflows 64 bits; must be rejected, not wrap around. */
    ZV_EXPECT_EQ(-EFAULT, zv_guest_memory_as_slice(memory, UINT64_MAX - 7, 16, &slice));
    ZV_EXPECT_EQ(-EFAULT, zv_guest_memory_as_slice(memory, 0x10, SIZE_MAX, &slice));

    zv_guest_memory_deinit(memory);
}

static void test_write_spans_regions(void)
{
    uint8_t ram_a[REGION_SIZE] = {0};
    uint8_t ram_b[REGION_SIZE] = {0};
    uint8_t data[32];
    struct zv_guest_memory *memory = NULL;

    memset(data, 0xab, sizeof(data));

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x0, ram_a, REGION_SIZE, false));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x1000, ram_b, REGION_SIZE, false));

    /* 16 bytes at the end of region a, 16 at the start of region b. */
    ZV_EXPECT_EQ(0, zv_guest_memory_write(memory, 0xff0, data, sizeof(data)));
    ZV_EXPECT_EQ(0xab, ram_a[0xff0]);
    ZV_EXPECT_EQ(0xab, ram_a[0xfff]);
    ZV_EXPECT_EQ(0xab, ram_b[0x0]);
    ZV_EXPECT_EQ(0xab, ram_b[0xf]);
    ZV_EXPECT_EQ(0, ram_b[0x10]);

    /* Runs off the end of guest memory: the part that fits is written, then an error. */
    ZV_EXPECT_EQ(-EFAULT, zv_guest_memory_write(memory, 0x1ff8, data, sizeof(data)));
    ZV_EXPECT_EQ(0xab, ram_b[0xfff]);

    zv_guest_memory_deinit(memory);
}

static void test_deinit_unmaps_only_mmaped_regions(void)
{
    uint8_t stack_ram[REGION_SIZE];
    struct zv_guest_memory *memory = NULL;
    void *mapped = NULL;

    zv_mmap_tracker_start();

    ZV_EXPECT_EQ(0, zv_mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                            -1, 0, &mapped));
    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x0, mapped, REGION_SIZE, true));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x1000, stack_ram, REGION_SIZE, false));

    /* Unmapping stack_ram would abort in the tracker ("range wasn't mapped with zv_mmap()"). */
    zv_guest_memory_deinit(memory);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
}

static void test_regions_get_consecutive_slots(void)
{
    uint8_t ram[3][REGION_SIZE];
    struct zv_guest_memory *memory = NULL;

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));

    for (int i = 0; i < 3; i++)
        ZV_EXPECT_EQ(
            0, zv_guest_memory_add(memory, (uint64_t)i * REGION_SIZE, ram[i], REGION_SIZE, false));

    ZV_EXPECT_EQ(3, memory->region_count);
    for (int i = 0; i < 3; i++)
        ZV_EXPECT_EQ(i, memory->regions[i].slot);

    zv_guest_memory_deinit(memory);
}

static const struct zv_test tests[] = {
    {"as_slice checks the whole range", test_as_slice_checks_the_whole_range},
    {"write spans regions", test_write_spans_regions},
    {"deinit unmaps only mmaped regions", test_deinit_unmaps_only_mmaped_regions},
    {"regions get consecutive slots", test_regions_get_consecutive_slots},
};

const struct zv_test_suite zv_vmm_memory_suite = {"vmm.memory", tests, ZV_ARRAY_LEN(tests)};
