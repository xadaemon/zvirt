/*
 * Device MAC address. Port of utils/mac.zig.
 *
 * Differences from the Zig version:
 * - Zig returns WrongFormat or InvalidNumberOfOctets. Here both are -EINVAL.
 * - Each part must be exactly two hex digits. Zig's parseInt accepts a few more odd spellings of a
 *   two-character part (e.g. "+f"). strtoul() isn't used for the same reason: it would accept
 *   " f" or "0x" as an octet.
 */
#include "utils/mac.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* Returns the value of a hex digit, or -1 if `c` isn't one. */
static int hex_digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

struct zv_mac zv_mac_from_bytes(const uint8_t bytes[ZV_MAC_LEN])
{
    struct zv_mac mac;

    memcpy(mac.bytes, bytes, ZV_MAC_LEN);
    return mac;
}

int zv_mac_from_str(const char *text, struct zv_mac *out)
{
    struct zv_mac mac;
    const char *part = text;

    for (int octet = 0; octet < ZV_MAC_LEN; octet++) {
        int high = hex_digit_value(part[0]);
        int low = high < 0 ? -1 : hex_digit_value(part[1]);

        if (high < 0 || low < 0)
            return -EINVAL;

        mac.bytes[octet] = (uint8_t)(high * 16 + low);

        /* After two digits: a ':' before the next part, or the end of the string after the last. */
        char separator = part[2];
        bool last = octet == ZV_MAC_LEN - 1;

        if (last && separator != '\0')
            return -EINVAL;
        if (!last && separator != ':')
            return -EINVAL;

        part += 3;
    }

    *out = mac;
    return 0;
}
