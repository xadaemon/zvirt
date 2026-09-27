/*
 * x86 MMIO bus. Port of vmm/arch/x86/mmio_bus.zig.
 *
 * Deviation: an access to an unmapped address panics in Zig ("todo"). The guest controls the
 * address, so here it logs an error and returns -EFAULT, which stops the vCPU.
 */
#include "vmm/arch/x86/mmio_bus.h"

#include "utils/log.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#define LOG_SCOPE "mmio_bus"

/* AMD FCH_PM_S5_RESET_STATUS. Holds the last crash reason; all ones means "unsupported". */
#define AMD_FCH_PM_S5_RESET_STATUS 0xfed803c0

static bool range_contains(const struct zv_mmio_range *range, uint64_t address)
{
    return address >= range->start && address - range->start < range->size;
}

static bool ranges_overlap(uint64_t a_start, uint64_t a_size, uint64_t b_start, uint64_t b_size)
{
    /* Both ranges are known not to wrap, so the end addresses can't overflow. */
    return a_start < b_start + b_size && b_start < a_start + a_size;
}

/* The legacy VGA (0xa0000-0xbffff) and BIOS (0xc0000-0xfffff) areas, and unmodelled registers. */
static bool reads_as_all_ones(uint64_t address)
{
    if (address >= 0xa0000 && address <= 0xfffff)
        return true;

    return address == AMD_FCH_PM_S5_RESET_STATUS;
}

void zv_mmio_bus_init(struct zv_mmio_bus *bus)
{
    bus->range_count = 0;
}

void zv_mmio_bus_deinit(struct zv_mmio_bus *bus)
{
    bus->range_count = 0;
}

int zv_mmio_bus_register_range(struct zv_mmio_bus *bus, uint64_t start, uint64_t size,
                               struct zv_mmio_device device)
{
    if (size == 0 || start + size < start)
        return -EINVAL;

    for (size_t i = 0; i < bus->range_count; i++) {
        const struct zv_mmio_range *existing = &bus->ranges[i];

        if (ranges_overlap(start, size, existing->start, existing->size))
            return -EEXIST;
    }

    if (bus->range_count == ZV_MMIO_BUS_MAX_RANGES)
        return -ENOSPC;

    struct zv_mmio_range *range = &bus->ranges[bus->range_count];

    range->start = start;
    range->size = size;
    range->device = device;
    bus->range_count++;
    return 0;
}

static struct zv_mmio_range *find_range(struct zv_mmio_bus *bus, uint64_t address)
{
    for (size_t i = 0; i < bus->range_count; i++) {
        if (range_contains(&bus->ranges[i], address))
            return &bus->ranges[i];
    }

    return NULL;
}

int zv_mmio_bus_handle_mmio(struct zv_mmio_bus *bus, const struct zv_kvm_mmio_exit *mmio,
                            struct zv_kvm_io_result *result)
{
    result->kind = ZV_KVM_IO_RESULT_NONE;

    if (reads_as_all_ones(mmio->physical_address)) {
        result->kind = ZV_KVM_IO_RESULT_MMIO;
        result->mmio_data = UINT64_MAX;
        return 0;
    }

    /* The data travels in a 64-bit value, so an access can't be wider (Zig slices would panic). */
    if (mmio->length > sizeof(uint64_t)) {
        zv_log_err(LOG_SCOPE, "MMIO access of %zu bytes at 0x%llx", mmio->length,
                   (unsigned long long)mmio->physical_address);
        return -EFAULT;
    }

    struct zv_mmio_range *range = find_range(bus, mmio->physical_address);

    if (range == NULL) {
        zv_log_err(LOG_SCOPE, "unhandled MMIO address 0x%llx",
                   (unsigned long long)mmio->physical_address);
        return -EFAULT;
    }

    uint64_t offset = mmio->physical_address - range->start;
    struct zv_mmio_device *device = &range->device;

    if (mmio->is_write) {
        uint8_t bytes[sizeof(uint64_t)];

        memcpy(bytes, &mmio->data, sizeof(bytes));
        return device->write(device->context, offset, bytes, mmio->length);
    }

    /* The device fills the first `length` bytes; the rest of the value stays zero. */
    uint8_t bytes[sizeof(uint64_t)] = {0};
    int err = device->read(device->context, offset, bytes, mmio->length);

    if (err < 0)
        return err;

    result->kind = ZV_KVM_IO_RESULT_MMIO;
    memcpy(&result->mmio_data, bytes, sizeof(bytes));
    return 0;
}
