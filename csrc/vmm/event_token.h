/*
 * Event tokens for the VM's main epoll loop. Port of EventSource and EventToken in
 * vmm/root.zig.
 *
 * Every fd the main loop watches carries a 64-bit token saying who owns it. This lives in its own
 * header, apart from vmm.h, because the buses need it and vmm.h includes the buses.
 */
#ifndef ZV_VMM_EVENT_TOKEN_H
#define ZV_VMM_EVENT_TOKEN_H

#include <stdint.h>

/* Who an event belongs to. Fits in 3 bits. */
enum zv_event_source {
    ZV_EVENT_SOURCE_VCPU = 0,
    ZV_EVENT_SOURCE_IO_BUS = 1,
    ZV_EVENT_SOURCE_VIRTIO = 2,
    ZV_EVENT_SOURCE_PCI = 3,
};

/* Largest id a token can carry (29 bits). */
#define ZV_EVENT_TOKEN_MAX_ID ((1u << 29) - 1)

struct zv_event_token {
    /* Index of the vCPU, COM port or device. */
    uint32_t id;
    int fd;
    enum zv_event_source source;
};

/*
 * Packs a token as Zig's packed struct(u64) does: id in bits 0-28, fd in bits 29-60, source in
 * bits 61-63. Aborts if the id doesn't fit.
 */
uint64_t zv_event_token_encode(struct zv_event_token token);

struct zv_event_token zv_event_token_decode(uint64_t value);

#endif
