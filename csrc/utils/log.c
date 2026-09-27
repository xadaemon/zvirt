/*
 * Logging with a scope name and a runtime level.
 */
#include "utils/log.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>

/*
 * Both values are read from vCPU threads, so they are atomics. Relaxed ordering is enough: the
 * level is set before threads start, and the error count is only compared after a test finishes.
 */
static atomic_int current_level = ZV_LOG_INFO;
static atomic_size_t error_count = 0;

static const char *level_name(enum zv_log_level level)
{
    switch (level) {
    case ZV_LOG_ERR:
        return "error";
    case ZV_LOG_WARN:
        return "warning";
    case ZV_LOG_INFO:
        return "info";
    case ZV_LOG_DEBUG:
        return "debug";
    }

    return "unknown";
}

void zv_log_set_level(enum zv_log_level level)
{
    atomic_store_explicit(&current_level, (int)level, memory_order_relaxed);
}

enum zv_log_level zv_log_get_level(void)
{
    return (enum zv_log_level)atomic_load_explicit(&current_level, memory_order_relaxed);
}

size_t zv_log_error_count(void)
{
    return atomic_load_explicit(&error_count, memory_order_relaxed);
}

void zv_log_write(enum zv_log_level level, const char *scope, const char *format, ...)
{
    /* Count errors before filtering, so a test cannot hide an error by lowering the level. */
    if (level == ZV_LOG_ERR)
        atomic_fetch_add_explicit(&error_count, 1, memory_order_relaxed);

    if ((int)level > atomic_load_explicit(&current_level, memory_order_relaxed))
        return;

    va_list args;

    va_start(args, format);

    /* Hold the stderr lock so lines from different threads don't interleave. */
    flockfile(stderr);
    fprintf(stderr, "%s(%s): ", level_name(level), scope);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    funlockfile(stderr);

    va_end(args);
}
