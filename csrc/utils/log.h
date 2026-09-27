/*
 * Logging with a scope name and a runtime level.
 *
 * C replacement for std.log.scoped() plus the runtime level filter in main.zig. Each file defines
 * its scope once and passes it to the log macros:
 *
 *     #define LOG_SCOPE "vmm"
 *     zv_log_info(LOG_SCOPE, "vCPU %d started", id);
 *
 * Output goes to stderr in the same format as Zig's default logger: "info(vmm): message".
 */
#ifndef ZV_UTILS_LOG_H
#define ZV_UTILS_LOG_H

#include <stddef.h>

enum zv_log_level {
    ZV_LOG_ERR = 0,
    ZV_LOG_WARN = 1,
    ZV_LOG_INFO = 2,
    ZV_LOG_DEBUG = 3,
};

/* Messages above this level are dropped. The default is ZV_LOG_INFO. */
void zv_log_set_level(enum zv_log_level level);
enum zv_log_level zv_log_get_level(void);

/*
 * Number of error messages logged so far, whether or not they were printed. The test runner uses
 * it to fail any test that logs an error, like test_runner.zig does.
 */
size_t zv_log_error_count(void);

void zv_log_write(enum zv_log_level level, const char *scope, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/* The format string is part of __VA_ARGS__, so calls without arguments stay valid C17. */
#define zv_log_err(scope, ...) zv_log_write(ZV_LOG_ERR, scope, __VA_ARGS__)
#define zv_log_warn(scope, ...) zv_log_write(ZV_LOG_WARN, scope, __VA_ARGS__)
#define zv_log_info(scope, ...) zv_log_write(ZV_LOG_INFO, scope, __VA_ARGS__)
#define zv_log_debug(scope, ...) zv_log_write(ZV_LOG_DEBUG, scope, __VA_ARGS__)

#endif
