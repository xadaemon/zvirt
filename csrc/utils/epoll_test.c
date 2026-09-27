/*
 * Tests for utils/epoll.c. Port of the test blocks in utils/epoll.zig.
 */
#include "utils/epoll.h"

#include "test_runner.h"

#include <sys/eventfd.h>
#include <unistd.h>

static void test_reports_eventfd_context(void)
{
    struct zv_epoll epoll;

    ZV_EXPECT_EQ(0, zv_epoll_new(&epoll));

    /* A raw eventfd, as in the Zig test, so this doesn't depend on utils/eventfd.c. */
    int event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    ZV_EXPECT(event_fd >= 0);

    const uint64_t expected_context = 0x123456789abcdef0;

    ZV_EXPECT_EQ(0, zv_epoll_add(&epoll, event_fd, expected_context));

    uint64_t value = 1;

    ZV_EXPECT_EQ(sizeof(value), write(event_fd, &value, sizeof(value)));

    struct epoll_event events[1];
    int ready = zv_epoll_wait(&epoll, events, ZV_ARRAY_LEN(events), 1000);

    ZV_EXPECT_EQ(1, ready);
    ZV_EXPECT((events[0].events & EPOLLIN) != 0);

    /* Read the packed field by value. */
    uint64_t context = events[0].data.u64;

    ZV_EXPECT(context == expected_context);

    close(event_fd);
    zv_epoll_deinit(&epoll);
}

static void test_zero_timeout_returns_no_events(void)
{
    struct zv_epoll epoll;
    struct epoll_event events[1];

    ZV_EXPECT_EQ(0, zv_epoll_new(&epoll));
    ZV_EXPECT_EQ(0, zv_epoll_wait(&epoll, events, ZV_ARRAY_LEN(events), 0));

    zv_epoll_deinit(&epoll);
}

static const struct zv_test tests[] = {
    {"epoll reports an eventfd context", test_reports_eventfd_context},
    {"epoll zero timeout returns no events", test_zero_timeout_returns_no_events},
};

const struct zv_test_suite zv_utils_epoll_suite = {"utils.epoll", tests, ZV_ARRAY_LEN(tests)};
