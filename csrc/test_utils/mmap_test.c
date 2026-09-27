/*
 * Tests for test_utils/mmap.c. Port of the test blocks in test_utils/mmap.zig.
 */
#include "test_utils/mmap.h"

#include "test_runner.h"

#include <sys/mman.h>

#define PAGE 4096
#define ANONYMOUS (MAP_PRIVATE | MAP_ANONYMOUS)

static void test_mmap_detector_works(void)
{
    /* Starting and stopping with nothing mapped reports no leaks. */
    zv_mmap_tracker_start();
    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());

    /* A mapping that is unmapped again is not a leak. */
    void *mapping = NULL;

    zv_mmap_tracker_start();
    ZV_EXPECT_EQ(0, zv_mmap(NULL, PAGE, PROT_READ | PROT_WRITE, ANONYMOUS, -1, 0, &mapping));
    zv_munmap(mapping, PAGE);
    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());

    /* A mapping that is never unmapped is reported. */
    zv_mmap_tracker_start();
    ZV_EXPECT_EQ(0, zv_mmap(NULL, PAGE, PROT_READ | PROT_WRITE, ANONYMOUS, -1, 0, &mapping));
    ZV_EXPECT_EQ(1, zv_mmap_tracker_stop());

    zv_munmap(mapping, PAGE);
}

static void test_mmap_failure_returns_errno(void)
{
    void *mapping = NULL;

    /* A zero-length mapping is rejected by the kernel with EINVAL. */
    int rc = zv_mmap(NULL, 0, PROT_READ, ANONYMOUS, -1, 0, &mapping);

    ZV_EXPECT(rc < 0);
    ZV_EXPECT(mapping == NULL);
}

static const struct zv_test tests[] = {
    {"mmap detector works", test_mmap_detector_works},
    {"mmap failure returns -errno", test_mmap_failure_returns_errno},
};

const struct zv_test_suite zv_test_utils_mmap_suite = {"test_utils.mmap", tests,
                                                       ZV_ARRAY_LEN(tests)};
