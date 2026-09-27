/*
 * Tests for utils/eventfd.c. Port of the test blocks in utils/eventfd.zig.
 */
#include "utils/eventfd.h"

#include "test_runner.h"

#include <errno.h>

static void test_accumulates_notifications(void)
{
    struct zv_eventfd eventfd;
    uint64_t value = 0;

    ZV_EXPECT_EQ(0, zv_eventfd_new(0, &eventfd));

    ZV_EXPECT_EQ(0, zv_eventfd_notify(&eventfd));
    ZV_EXPECT_EQ(0, zv_eventfd_write(&eventfd, 2));
    ZV_EXPECT_EQ(0, zv_eventfd_read(&eventfd, &value));
    ZV_EXPECT_EQ(3, value);

    /* The read reset the counter, so a second read would block. */
    ZV_EXPECT_EQ(-EAGAIN, zv_eventfd_read(&eventfd, &value));

    zv_eventfd_deinit(&eventfd);
}

static void test_semaphore_reads_one_at_a_time(void)
{
    struct zv_eventfd eventfd;
    uint64_t value = 0;

    ZV_EXPECT_EQ(0, zv_eventfd_new_semaphore(2, &eventfd));

    ZV_EXPECT_EQ(0, zv_eventfd_read(&eventfd, &value));
    ZV_EXPECT_EQ(1, value);
    ZV_EXPECT_EQ(0, zv_eventfd_read(&eventfd, &value));
    ZV_EXPECT_EQ(1, value);
    ZV_EXPECT_EQ(-EAGAIN, zv_eventfd_read(&eventfd, &value));

    zv_eventfd_deinit(&eventfd);
}

static const struct zv_test tests[] = {
    {"eventfd accumulates notifications", test_accumulates_notifications},
    {"eventfd semaphore reads one notification at a time", test_semaphore_reads_one_at_a_time},
};

const struct zv_test_suite zv_utils_eventfd_suite = {"utils.eventfd", tests, ZV_ARRAY_LEN(tests)};
