/*
 * x86 device bus. Port of vmm/arch/x86/device_bus.zig.
 */
#include "vmm/arch/x86/device_bus.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void zv_device_bus_init(struct zv_device_bus *bus)
{
    zv_mmio_bus_init(&bus->mmio_bus);
    zv_io_bus_init(&bus->io_bus);
}

void zv_device_bus_deinit(struct zv_device_bus *bus)
{
    zv_mmio_bus_deinit(&bus->mmio_bus);
    zv_io_bus_deinit(&bus->io_bus);
}

int zv_device_bus_attach_console(struct zv_device_bus *bus,
                                 const struct zv_vm_console_config *console, struct zv_vm *vm)
{
    return zv_io_bus_attach_console(&bus->io_bus, console, vm);
}

int zv_device_bus_handle_event(struct zv_device_bus *bus, enum zv_event_source source, uint32_t id,
                               int fd)
{
    (void)fd; /* Used by the virtio and PCI devices (phases 6 and 7). */

    switch (source) {
    case ZV_EVENT_SOURCE_IO_BUS:
        return zv_io_bus_handle_event(&bus->io_bus, id);
    case ZV_EVENT_SOURCE_VIRTIO:
    case ZV_EVENT_SOURCE_PCI:
        /* No virtio or PCI devices can be created until phases 6 and 7 (see zv_vm_new). */
        return -ENOTSUP;
    case ZV_EVENT_SOURCE_VCPU:
        break;
    }

    /* vCPU events are handled by the VM itself (unreachable in Zig). */
    fprintf(stderr, "device bus: unexpected event source %d\n", (int)source);
    abort();
}

int zv_device_bus_handle_io(struct zv_device_bus *bus, const struct zv_kvm_io_exit *io,
                            bool *test_exit)
{
    return zv_io_bus_handle_io(&bus->io_bus, io, test_exit);
}

int zv_device_bus_handle_mmio(struct zv_device_bus *bus, const struct zv_kvm_mmio_exit *mmio,
                              struct zv_kvm_io_result *result)
{
    return zv_mmio_bus_handle_mmio(&bus->mmio_bus, mmio, result);
}
