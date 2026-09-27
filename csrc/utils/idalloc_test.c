/*
 * Tests for utils/idalloc.c. Port of the test block in utils/idalloc.zig, plus a test for
 * zv_id_allocator_allocate_specific(), which the x86 VM setup uses to reserve fixed IRQs.
 */
#include "utils/idalloc.h"

#include "test_runner.h"

#include <errno.h>

static void test_allocate_and_free(void)
{
    /* 10 isn't a multiple of 8, as in the Zig test, whose bitmap was an array of bytes. */
    struct zv_id_allocator allocator;
    size_t id = 0;

    zv_id_allocator_init(&allocator, 10);

    for (size_t i = 0; i < 10; i++) {
        ZV_EXPECT_EQ(0, zv_id_allocator_allocate(&allocator, &id));
        ZV_EXPECT_EQ(i, id);
    }

    ZV_EXPECT_EQ(-ENOSPC, zv_id_allocator_allocate(&allocator, &id));

    zv_id_allocator_free(&allocator, 0);
    ZV_EXPECT_EQ(0, zv_id_allocator_allocate(&allocator, &id));
    ZV_EXPECT_EQ(0, id);

    for (size_t i = 0; i < 10; i++)
        zv_id_allocator_free(&allocator, i);

    for (size_t i = 0; i < 10; i++) {
        ZV_EXPECT_EQ(0, zv_id_allocator_allocate(&allocator, &id));
        ZV_EXPECT_EQ(i, id);
    }
}

static void test_allocate_specific(void)
{
    struct zv_id_allocator allocator;
    size_t id = 0;

    zv_id_allocator_init(&allocator, 16);

    ZV_EXPECT_EQ(0, zv_id_allocator_allocate_specific(&allocator, 4));
    ZV_EXPECT_EQ(-EBUSY, zv_id_allocator_allocate_specific(&allocator, 4));

    /* Plain allocation skips the reserved id. */
    ZV_EXPECT_EQ(0, zv_id_allocator_allocate_specific(&allocator, 0));
    ZV_EXPECT_EQ(0, zv_id_allocator_allocate(&allocator, &id));
    ZV_EXPECT_EQ(1, id);
}

static const struct zv_test tests[] = {
    {"allocate and free", test_allocate_and_free},
    {"allocate specific", test_allocate_specific},
};

const struct zv_test_suite zv_utils_idalloc_suite = {"utils.idalloc", tests, ZV_ARRAY_LEN(tests)};
