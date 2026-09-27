/*
 * Line-oriented test runner. Port of test_runner.zig.
 *
 * Each test runs in a forked child process, which gives every test a fresh copy of global state
 * (signal handlers, open fds, the log error counter) the same way the Zig runner resets its
 * allocator and io instance between tests. The child reports its result through its exit code.
 */
#include "test_runner.h"

#include "utils/log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* LeakSanitizer is part of AddressSanitizer. In tsan and release builds there is no leak check. */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ZV_HAVE_LEAK_CHECK 1
#include <sanitizer/lsan_interface.h>
#endif
#endif

/* Exit codes a test child uses to report its result to the parent. */
enum child_result {
    CHILD_PASS = 0,
    CHILD_FAIL = 1,
    CHILD_SKIP = 2,
    CHILD_LEAK = 3,
};

/* Tests run with warnings visible and info/debug hidden, like test_runner.zig. */
#define TEST_LOG_LEVEL ZV_LOG_WARN

/*
 * The child must leave with _exit(), never exit(). exit() would run the atexit handlers and
 * LeakSanitizer's end-of-process check a second time. _exit() skips stdio flushing, so flush first.
 */
_Noreturn static void child_exit(enum child_result result)
{
    fflush(NULL);
    _exit(result);
}

_Noreturn void zv_test_fail_at(const char *file, int line, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    fprintf(stderr, "ERROR %s:%d: ", file, line);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);

    child_exit(CHILD_FAIL);
}

_Noreturn void zv_test_skip(const char *reason)
{
    fprintf(stderr, "SKIPPING: %s\n", reason);
    child_exit(CHILD_SKIP);
}

static bool leak_check_found_leaks(void)
{
#ifdef ZV_HAVE_LEAK_CHECK
    return __lsan_do_recoverable_leak_check() != 0;
#else
    return false;
#endif
}

/* Runs inside the forked child. Never returns. */
_Noreturn static void run_test_in_child(const struct zv_test *test)
{
    zv_log_set_level(TEST_LOG_LEVEL);
    size_t errors_before = zv_log_error_count();

    test->fn();

    if (zv_log_error_count() != errors_before) {
        fprintf(stderr, "ERROR: test logged an error\n");
        child_exit(CHILD_FAIL);
    }

    if (leak_check_found_leaks())
        child_exit(CHILD_LEAK);

    child_exit(CHILD_PASS);
}

/* Forks, runs the test in the child and turns the child's exit status into a result. */
static enum child_result run_test(const struct zv_test *test)
{
    /* Anything still buffered would otherwise be printed by both processes. */
    fflush(NULL);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return CHILD_FAIL;
    }

    if (pid == 0)
        run_test_in_child(test);

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return CHILD_FAIL;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "ERROR: test killed by signal %d (%s)\n", WTERMSIG(status),
                strsignal(WTERMSIG(status)));
        return CHILD_FAIL;
    }

    switch (WEXITSTATUS(status)) {
    case CHILD_PASS:
        return CHILD_PASS;
    case CHILD_SKIP:
        return CHILD_SKIP;
    case CHILD_LEAK:
        return CHILD_LEAK;
    default:
        /* CHILD_FAIL, or a sanitizer that exited on its own after reporting an error. */
        return CHILD_FAIL;
    }
}

static void full_test_name(char *buffer, size_t size, const struct zv_test_suite *suite,
                           const struct zv_test *test)
{
    snprintf(buffer, size, "%s: %s", suite->name, test->name);
}

static bool is_selected(const char *name, int filter_count, char **filters)
{
    if (filter_count == 0)
        return true;

    for (int i = 0; i < filter_count; i++) {
        if (strstr(name, filters[i]) != NULL)
            return true;
    }

    return false;
}

int zv_test_main(int argc, char **argv, const struct zv_test_suite *const *suites,
                 size_t suite_count)
{
    /* Every command-line argument is a filter. */
    int filter_count = argc - 1;
    char **filters = argv + 1;
    char name[512];

    /* Count the selected tests first, so START lines can show "i/N". */
    size_t total = 0;

    for (size_t s = 0; s < suite_count; s++) {
        for (size_t t = 0; t < suites[s]->count; t++) {
            full_test_name(name, sizeof(name), suites[s], &suites[s]->tests[t]);
            if (is_selected(name, filter_count, filters))
                total++;
        }
    }

    size_t index = 0;
    size_t passed = 0;
    size_t skipped = 0;
    size_t failed = 0;
    size_t leaked = 0;

    for (size_t s = 0; s < suite_count; s++) {
        for (size_t t = 0; t < suites[s]->count; t++) {
            const struct zv_test *test = &suites[s]->tests[t];

            full_test_name(name, sizeof(name), suites[s], test);
            if (!is_selected(name, filter_count, filters))
                continue;

            index++;
            fprintf(stderr, "START %zu/%zu %s\n", index, total, name);

            const char *label = "PASS";

            switch (run_test(test)) {
            case CHILD_PASS:
                passed++;
                break;
            case CHILD_SKIP:
                skipped++;
                label = "SKIP";
                break;
            case CHILD_LEAK:
                /* A leak also fails the test, as in test_runner.zig. */
                leaked++;
                failed++;
                label = "FAIL";
                break;
            case CHILD_FAIL:
                failed++;
                label = "FAIL";
                break;
            }

            fprintf(stderr, "%s %zu/%zu %s\n", label, index, total, name);
        }
    }

    fprintf(stderr, "SUMMARY: %zu passed, %zu skipped, %zu failed, %zu leaked\n", passed, skipped,
            failed, leaked);

    return failed == 0 ? 0 : 1;
}
