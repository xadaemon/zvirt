/*
 * Device interfaces. Port of the MmioDevice type in vmm/device/root.zig (which otherwise only
 * re-exports the device modules).
 */
#ifndef ZV_VMM_DEVICE_DEVICE_H
#define ZV_VMM_DEVICE_DEVICE_H

#include <stddef.h>
#include <stdint.h>

/*
 * A device reachable through guest memory accesses. `offset` is relative to the start of the
 * device's range; `length` is 1 to 8 bytes. Both functions return 0 or -errno.
 */
struct zv_mmio_device {
    void *context;
    int (*read)(void *context, uint64_t offset, uint8_t *data, size_t length);
    int (*write)(void *context, uint64_t offset, const uint8_t *data, size_t length);
};

#endif
