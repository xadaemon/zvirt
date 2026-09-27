/*
 * Event tokens for the VM's main epoll loop. Port of EventSource and EventToken in
 * vmm/root.zig.
 */
#include "vmm/event_token.h"

#include <stdio.h>
#include <stdlib.h>

#define ID_BITS 29
#define FD_BITS 32

#define ID_SHIFT 0
#define FD_SHIFT ID_BITS
#define SOURCE_SHIFT (ID_BITS + FD_BITS)

#define ID_MASK (((uint64_t)1 << ID_BITS) - 1)
#define FD_MASK (((uint64_t)1 << FD_BITS) - 1)
#define SOURCE_MASK ((uint64_t)0x7)

uint64_t zv_event_token_encode(struct zv_event_token token)
{
    if (token.id > ZV_EVENT_TOKEN_MAX_ID) {
        fprintf(stderr, "event token: id %u doesn't fit in 29 bits\n", token.id);
        abort();
    }

    uint64_t id = token.id;
    uint64_t fd = (uint32_t)token.fd;
    uint64_t source = (uint64_t)token.source & SOURCE_MASK;

    return (id << ID_SHIFT) | (fd << FD_SHIFT) | (source << SOURCE_SHIFT);
}

struct zv_event_token zv_event_token_decode(uint64_t value)
{
    struct zv_event_token token;

    token.id = (uint32_t)((value >> ID_SHIFT) & ID_MASK);
    token.fd = (int)(uint32_t)((value >> FD_SHIFT) & FD_MASK);
    token.source = (enum zv_event_source)((value >> SOURCE_SHIFT) & SOURCE_MASK);
    return token;
}
