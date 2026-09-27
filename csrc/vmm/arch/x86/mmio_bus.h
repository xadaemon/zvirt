/*
 * x86 MMIO bus. Port of vmm/arch/x86/mmio_bus.zig.
 *
 * Routes guest accesses to unbacked addresses (MMIO exits) to the device registered for that
 * range. The Zig version keeps the ranges in a treap; a few devices at most are registered, so
 * here they're a fixed array searched linearly.
 *
 * The virtio-mmio device list (virtio_devs in Zig) is added in phase 6.
 */
#ifndef ZV_VMM_ARCH_X86_MMIO_BUS_H
#define ZV_VMM_ARCH_X86_MMIO_BUS_H

#include "kvm/vcpu.h"
#include "vmm/device/device.h"

#include <stddef.h>
#include <stdint.h>

#define ZV_MMIO_BUS_MAX_RANGES 32

struct zv_mmio_range {
    uint64_t start;
    uint64_t size;
    struct zv_mmio_device device;
};

struct zv_mmio_bus {
    struct zv_mmio_range ranges[ZV_MMIO_BUS_MAX_RANGES];
    size_t range_count;
};

void zv_mmio_bus_init(struct zv_mmio_bus *bus);

void zv_mmio_bus_deinit(struct zv_mmio_bus *bus);

/*
 * Routes accesses to [start, start + size) to `device`. Returns 0, -EEXIST if the range overlaps
 * one already registered, -EINVAL if it is empty or wraps, or -ENOSPC if the bus is full.
 */
int zv_mmio_bus_register_range(struct zv_mmio_bus *bus, uint64_t start, uint64_t size,
                               struct zv_mmio_device device);

/*
 * Handles one MMIO exit. For a read, `result` gets the value the guest will see; for a write it is
 * set to NONE. The legacy VGA/BIOS hole and a few unimplemented platform registers read as all
 * ones.
 *
 * Returns 0, the device's -errno, or -EFAULT (after logging an error) if nothing is mapped at the
 * address or the access is wider than 8 bytes.
 */
int zv_mmio_bus_handle_mmio(struct zv_mmio_bus *bus, const struct zv_kvm_mmio_exit *mmio,
                            struct zv_kvm_io_result *result);

#endif
