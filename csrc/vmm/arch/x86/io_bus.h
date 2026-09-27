/*
 * x86 I/O ports. Port of vmm/arch/x86/io_bus.zig.
 *
 * Routes port I/O exits: four COM ports (16550 UARTs), the CMOS/RTC, the PCI configuration ports,
 * a test-exit port, and a few legacy ports that are ignored.
 *
 * The PCI bus (pci_bus_obj in Zig) is added in phase 7. Until then the PCI ports behave as they do
 * in Zig when there is no bus.
 */
#ifndef ZV_VMM_ARCH_X86_IO_BUS_H
#define ZV_VMM_ARCH_X86_IO_BUS_H

#include "kvm/vcpu.h"
#include "vmm/device/cmos.h"
#include "vmm/device/uart_16550.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>

struct zv_vm;
struct zv_vm_console_config;

#define ZV_IO_BUS_MAX_COMS 4

struct zv_io_bus {
    /* COM port i exists when com_present[i]; com_mutex[i] guards com[i]. */
    pthread_mutex_t com_mutex[ZV_IO_BUS_MAX_COMS];
    bool com_present[ZV_IO_BUS_MAX_COMS];
    struct zv_uart com[ZV_IO_BUS_MAX_COMS];

    /* Terminal settings to restore on deinit, for consoles attached with configure_terminal. */
    bool termios_saved[ZV_IO_BUS_MAX_COMS];
    struct termios original_termios[ZV_IO_BUS_MAX_COMS];

    struct zv_cmos cmos;
    pthread_mutex_t cmos_lock;

    /* The PCI CONFIG_ADDRESS register (port 0xcf8), set by one vCPU and read by others. */
    atomic_uint address_port;
};

void zv_io_bus_init(struct zv_io_bus *bus);

/* Restores terminal settings and tears down the COM ports. */
void zv_io_bus_deinit(struct zv_io_bus *bus);

/*
 * Creates COM port `console->index` and, if it has an input fd, adds that fd to the VM's main
 * loop. Returns 0, -EINVAL for an index above 3, -EEXIST if the port exists, or another -errno.
 */
int zv_io_bus_attach_console(struct zv_io_bus *bus, const struct zv_vm_console_config *console,
                             struct zv_vm *vm);

/* Called from the main loop when COM port `id`'s input fd is readable. Returns 0 or -errno. */
int zv_io_bus_handle_event(struct zv_io_bus *bus, uint32_t id);

/*
 * Handles one port I/O exit. Sets *test_exit when the guest wrote to the test-exit port (0xf4).
 * Returns 0, or -errno (after logging an error for an unknown port) to stop the vCPU.
 */
int zv_io_bus_handle_io(struct zv_io_bus *bus, const struct zv_kvm_io_exit *io, bool *test_exit);

#endif
