/*
 * Device MAC address. Port of utils/mac.zig.
 */
#ifndef ZV_UTILS_MAC_H
#define ZV_UTILS_MAC_H

#include <stdint.h>

#define ZV_MAC_LEN 6

struct zv_mac {
    uint8_t bytes[ZV_MAC_LEN];
};

struct zv_mac zv_mac_from_bytes(const uint8_t bytes[ZV_MAC_LEN]);

/*
 * Parses "ff:ee:dd:cc:bb:aa": six parts separated by ':', each exactly two hex digits in either
 * case. Returns 0, or -EINVAL for any other input.
 */
int zv_mac_from_str(const char *text, struct zv_mac *out);

#endif
