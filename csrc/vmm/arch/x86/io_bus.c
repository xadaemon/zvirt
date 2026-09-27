/*
 * x86 I/O ports. Port of vmm/arch/x86/io_bus.zig.
 */
#include "vmm/arch/x86/io_bus.h"

#include "utils/log.h"
#include "vmm/vmm.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_SCOPE "io_bus"

/* Base ports of COM1-COM4. Each UART has 8 registers. */
static const uint16_t com_base_port[ZV_IO_BUS_MAX_COMS] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
#define COM_PORT_COUNT 8

/* Writing to this port ends a test guest (see test_bins/). */
#define TEST_EXIT_PORT 0xf4

#define CMOS_INDEX_PORT 0x70
#define CMOS_DATA_PORT 0x71

/* PCI configuration mechanism #1: an address register and a 4-byte data window. */
#define PCI_CONFIG_ADDRESS_PORT 0xcf8
#define PCI_CONFIG_DATA_PORT 0xcfc
#define PCI_CONFIG_DATA_LAST_PORT 0xcff
/* Configuration mechanism #2 register, and the byte holding CONFIG_ADDRESS's enable bit. */
#define PCI_MECHANISM_2_PORT 0xcfa
#define PCI_CONFIG_ADDRESS_ENABLE_BYTE_PORT 0xcfb

/* The enable bit of CONFIG_ADDRESS (bit 31), i.e. bit 7 of its top byte. */
#define PCI_ADDRESS_ENABLE_SHIFT 31

/* COM1 and COM3 use IRQ 4, COM2 and COM4 use IRQ 3. */
static uint32_t com_irq(size_t index)
{
    return (index == 0 || index == 2) ? 4 : 3;
}

static bool in_range(uint16_t port, uint16_t first, uint16_t last)
{
    return port >= first && port <= last;
}

/* Puts the console's terminal into raw mode, remembering the original settings. */
static int setup_terminal(struct zv_io_bus *bus, int fd, size_t index)
{
    struct termios original;

    if (tcgetattr(fd, &original) != 0)
        return -errno;

    struct termios raw = original;

    /* Deliver input without waiting for a newline. */
    raw.c_lflag &= (tcflag_t)~ICANON;
    /* Do not echo typed characters. */
    raw.c_lflag &= (tcflag_t)~ECHO;
    /* Pass Ctrl-C, Ctrl-Z, etc. to the guest instead of raising signals. */
    raw.c_lflag &= (tcflag_t)~ISIG;
    /* A read may return as soon as one byte is available. */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &raw) != 0)
        return -errno;

    assert(!bus->termios_saved[index]);
    bus->original_termios[index] = original;
    bus->termios_saved[index] = true;
    return 0;
}

void zv_io_bus_init(struct zv_io_bus *bus)
{
    for (size_t i = 0; i < ZV_IO_BUS_MAX_COMS; i++) {
        pthread_mutex_init(&bus->com_mutex[i], NULL);
        bus->com_present[i] = false;
        bus->termios_saved[i] = false;
    }

    zv_cmos_init(&bus->cmos);
    pthread_mutex_init(&bus->cmos_lock, NULL);
    atomic_init(&bus->address_port, 0);
}

void zv_io_bus_deinit(struct zv_io_bus *bus)
{
    for (size_t i = 0; i < ZV_IO_BUS_MAX_COMS; i++) {
        if (!bus->termios_saved[i])
            continue;

        /* A saved terminal always belongs to an existing COM port. */
        if (tcsetattr(bus->com[i].output_fd, TCSANOW, &bus->original_termios[i]) != 0) {
            perror("io_bus: failed to restore terminal");
            abort();
        }
    }

    for (size_t i = 0; i < ZV_IO_BUS_MAX_COMS; i++) {
        if (bus->com_present[i])
            zv_uart_deinit(&bus->com[i]);

        pthread_mutex_destroy(&bus->com_mutex[i]);
    }

    pthread_mutex_destroy(&bus->cmos_lock);
}

int zv_io_bus_attach_console(struct zv_io_bus *bus, const struct zv_vm_console_config *console,
                             struct zv_vm *vm)
{
    if (console->index >= ZV_IO_BUS_MAX_COMS)
        return -EINVAL;

    size_t index = console->index;

    if (bus->com_present[index])
        return -EEXIST;

    int err =
        zv_uart_init(&bus->com[index], console->input_fd, console->output_fd, com_irq(index), vm);

    if (err < 0)
        return err;

    /* As in the Zig version, the port stays attached if a later step fails. */
    bus->com_present[index] = true;

    if (console->input_fd >= 0) {
        err = zv_vm_register_fd(vm, console->input_fd, (uint32_t)index, ZV_EVENT_SOURCE_IO_BUS,
                                false);
        if (err < 0)
            return err;
    }

    if (console->configure_terminal)
        return setup_terminal(bus, console->output_fd, index);

    return 0;
}

int zv_io_bus_handle_event(struct zv_io_bus *bus, uint32_t id)
{
    if (id >= ZV_IO_BUS_MAX_COMS)
        return -EINVAL;

    int err = 0;

    pthread_mutex_lock(&bus->com_mutex[id]);
    if (bus->com_present[id])
        err = zv_uart_handle_event(&bus->com[id]);
    pthread_mutex_unlock(&bus->com_mutex[id]);

    return err;
}

static int handle_com(struct zv_io_bus *bus, const struct zv_kvm_io_exit *io, size_t index,
                      unsigned offset)
{
    if (io->size != 1)
        return -EINVAL;

    int err = 0;

    pthread_mutex_lock(&bus->com_mutex[index]);

    /* A port with nothing attached ignores the access, as in the Zig version. */
    if (bus->com_present[index]) {
        struct zv_uart *uart = &bus->com[index];

        if (io->direction == ZV_KVM_IO_OUT)
            err = zv_uart_write_reg(uart, offset, io->data[0]);
        else
            err = zv_uart_read_reg(uart, offset, &io->data[0]);
    }

    pthread_mutex_unlock(&bus->com_mutex[index]);
    return err;
}

static int handle_cmos(struct zv_io_bus *bus, const struct zv_kvm_io_exit *io)
{
    if (io->size != 1)
        return -EINVAL;

    enum zv_cmos_register reg = (enum zv_cmos_register)(io->port - CMOS_INDEX_PORT);
    int err;

    pthread_mutex_lock(&bus->cmos_lock);
    if (io->direction == ZV_KVM_IO_OUT)
        err = zv_cmos_write_reg(&bus->cmos, reg, io->data[0]);
    else
        err = zv_cmos_read_reg(&bus->cmos, reg, &io->data[0]);
    pthread_mutex_unlock(&bus->cmos_lock);

    return err;
}

/* What the guest sees when there is no PCI bus: every byte of the access is 0xff. */
static void pci_unsupported(const struct zv_kvm_io_exit *io)
{
    for (size_t i = 0; i < io->size; i++)
        io->data[i] = 0xff;
}

static int handle_pci_enable_byte(struct zv_io_bus *bus, const struct zv_kvm_io_exit *io)
{
    if (io->size != 1)
        return -EINVAL;

    unsigned address = atomic_load_explicit(&bus->address_port, memory_order_relaxed);

    if (io->direction == ZV_KVM_IO_OUT) {
        unsigned enable = (unsigned)(io->data[0] >> 7);

        address &= ~(1u << PCI_ADDRESS_ENABLE_SHIFT);
        address |= enable << PCI_ADDRESS_ENABLE_SHIFT;
        atomic_store_explicit(&bus->address_port, address, memory_order_relaxed);
    } else {
        io->data[0] = (uint8_t)((address >> PCI_ADDRESS_ENABLE_SHIFT) << 7);
    }

    return 0;
}

int zv_io_bus_handle_io(struct zv_io_bus *bus, const struct zv_kvm_io_exit *io, bool *test_exit)
{
    uint16_t port = io->port;

    *test_exit = false;

    for (size_t i = 0; i < ZV_IO_BUS_MAX_COMS; i++) {
        uint16_t base = com_base_port[i];

        if (in_range(port, base, base + COM_PORT_COUNT - 1))
            return handle_com(bus, io, i, port - base);
    }

    if (port == TEST_EXIT_PORT) {
        *test_exit = true;
        return 0;
    }

    /* No floppy (0x3f0-0x3f7), no POST diagnostics (0x80), no PS/2 controller (0x64). */
    if (in_range(port, 0x3f0, 0x3f7) || port == 0x80 || port == 0x64) {
        if (io->size != 1)
            return -EINVAL;

        /* As in the Zig version: fills the data for OUT, leaves it untouched for IN. */
        if (io->direction == ZV_KVM_IO_OUT)
            io->data[0] = 0xff;

        return 0;
    }

    if (in_range(port, CMOS_INDEX_PORT, CMOS_DATA_PORT))
        return handle_cmos(bus, io);

    /* DMA page register: todo. */
    if (port == 0x87)
        return 0;

    if (port == PCI_CONFIG_ADDRESS_PORT) {
        /* No PCI bus until phase 7. */
        pci_unsupported(io);
        return 0;
    }

    /* Linux only uses configuration mechanism #1 when a PCI bus exists. */
    if (port == PCI_MECHANISM_2_PORT) {
        pci_unsupported(io);
        return 0;
    }

    if (port == PCI_CONFIG_ADDRESS_ENABLE_BYTE_PORT)
        return handle_pci_enable_byte(bus, io);

    if (in_range(port, PCI_CONFIG_DATA_PORT, PCI_CONFIG_DATA_LAST_PORT)) {
        /* No PCI bus until phase 7. */
        pci_unsupported(io);
        return 0;
    }

    zv_log_err(LOG_SCOPE, "unknown port access: port 0x%x, %s, size %u, count %u", port,
               io->direction == ZV_KVM_IO_OUT ? "out" : "in", io->size, io->count);
    return -EINVAL;
}
