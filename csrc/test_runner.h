/*
 * Test runner: C replacement for Zig's `test` blocks and test_runner.zig.
 *
 * A test is a `void f(void)` function. It passes if it returns normally. The ZV_EXPECT macros end
 * the test on the first failed check. That is safe because every test runs in its own forked
 * process, so nothing leaks into the next test.
 *
 * Tests are registered in explicit tables, one suite per source file:
 *
 *     static void test_something(void) { ZV_EXPECT(1 + 1 == 2); }
 *
 *     static const struct zv_test tests[] = {
 *         {"something works", test_something},
 *     };
 *     const struct zv_test_suite zv_foo_suite = {"foo", tests, ZV_ARRAY_LEN(tests)};
 *
 * Each test executable has a small main() that lists its suites and calls zv_test_main().
 */
#ifndef ZV_TEST_RUNNER_H
#define ZV_TEST_RUNNER_H

#include <stddef.h>

#define ZV_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

struct zv_test {
    const char *name;
    void (*fn)(void);
};

struct zv_test_suite {
    const char *name;
    const struct zv_test *tests;
    size_t count;
};

/*
 * Runs every test whose full name ("suite: test") contains any of the filters given as command-line
 * arguments, or every test when there are none. Prints the same START/PASS/SKIP/FAIL/SUMMARY lines
 * as test_runner.zig. Returns the process exit code: 0 if nothing failed, 1 otherwise.
 */
int zv_test_main(int argc, char **argv, const struct zv_test_suite *const *suites,
                 size_t suite_count);

/* Ends the current test as failed. Use the ZV_EXPECT macros rather than calling this directly. */
_Noreturn void zv_test_fail_at(const char *file, int line, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/* Ends the current test as skipped, e.g. when /dev/kvm is missing. */
_Noreturn void zv_test_skip(const char *reason);

#define ZV_EXPECT(condition)                                                                       \
    do {                                                                                           \
        if (!(condition))                                                                          \
            zv_test_fail_at(__FILE__, __LINE__, "expected: %s", #condition);                       \
    } while (0)

/* Compares two integers. Both are converted to long long so the message can print them. */
#define ZV_EXPECT_EQ(expected, actual)                                                             \
    do {                                                                                           \
        long long expected_value_ = (long long)(expected);                                         \
        long long actual_value_ = (long long)(actual);                                             \
        if (expected_value_ != actual_value_)                                                      \
            zv_test_fail_at(__FILE__, __LINE__, "expected %s == %s, got %lld and %lld", #expected, \
                            #actual, expected_value_, actual_value_);                              \
    } while (0)

#endif
