/*
 * x86 device bus. Port of vmm/arch/x86/device_bus.zig.
 *
 * Owns the I/O port bus and the MMIO bus, and routes vCPU exits and main-loop events to them.
 * Device creation (setup_devices: virtio block and net) comes in phases 6 to 8.
 */
#ifndef ZV_VMM_ARCH_X86_DEVICE_BUS_H
#define ZV_VMM_ARCH_X86_DEVICE_BUS_H

#include "kvm/vcpu.h"
#include "vmm/arch/x86/io_bus.h"
#include "vmm/arch/x86/mmio_bus.h"
#include "vmm/event_token.h"

#include <stdbool.h>
#include <stdint.h>

struct zv_vm;
struct zv_vm_console_config;

struct zv_device_bus {
    struct zv_io_bus io_bus;
    struct zv_mmio_bus mmio_bus;
};

void zv_device_bus_init(struct zv_device_bus *bus);
void zv_device_bus_deinit(struct zv_device_bus *bus);

int zv_device_bus_attach_console(struct zv_device_bus *bus,
                                 const struct zv_vm_console_config *console, struct zv_vm *vm);

/* Routes a main-loop event for a device fd. Returns 0 or -errno. */
int zv_device_bus_handle_event(struct zv_device_bus *bus, enum zv_event_source source, uint32_t id,
                               int fd);

int zv_device_bus_handle_io(struct zv_device_bus *bus, const struct zv_kvm_io_exit *io,
                            bool *test_exit);

int zv_device_bus_handle_mmio(struct zv_device_bus *bus, const struct zv_kvm_mmio_exit *mmio,
                              struct zv_kvm_io_result *result);

#endif
