/*
 * Whole-file reading. New; replaces std.Io.Dir.readFileAlloc(), which has no C equivalent.
 */
#ifndef ZV_UTILS_FILE_H
#define ZV_UTILS_FILE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Reads the whole file at `path` into a malloc()ed buffer, which the caller frees. Returns 0, or
 * -errno (nothing is allocated on failure).
 */
int zv_read_file(const char *path, uint8_t **data, size_t *size);

#endif
