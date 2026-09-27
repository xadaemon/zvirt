/*
 * Virtual 16550 UART (serial port). Port of vmm/device/uart_16550.zig.
 *
 * Eight registers at consecutive I/O ports. Output bytes are written straight to `output_fd`, so
 * the transmitter is always empty. Input bytes read from `input_fd` wait in a receive queue until
 * the guest reads them. Interrupts are raised through an eventfd registered with KVM.
 *
 * Registers are plain bytes; the bit layouts Zig expressed as packed structs are macros in
 * uart_16550.c.
 */
#ifndef ZV_VMM_DEVICE_UART_16550_H
#define ZV_VMM_DEVICE_UART_16550_H

#include "utils/eventfd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct zv_vm;

#define ZV_UART_RX_CAPACITY 4096

struct zv_uart {
    /* Host fd the guest's input comes from, or -1 for none. */
    int input_fd;
    /* Host fd the guest's output goes to. */
    int output_fd;

    uint8_t lcr; /* Line control */
    uint8_t scr; /* Scratch */
    uint8_t mcr; /* Modem control */
    uint8_t ier; /* Interrupt enable */
    uint8_t dll; /* Divisor latch, low byte */
    uint8_t dlh; /* Divisor latch, high byte */
    uint8_t lsr; /* Line status */
    uint8_t iir; /* Interrupt identification */

    /* A "transmitter empty" interrupt is owed to the guest. */
    bool thre_pending;

    /* Receive queue: a ring buffer of `rx_count` bytes starting at `rx_head`. */
    uint8_t rx_storage[ZV_UART_RX_CAPACITY];
    size_t rx_head;
    size_t rx_count;

    uint32_t irq;
    struct zv_eventfd irqfd;
    /* The interrupt was raised and not yet cleared. */
    bool irq_set;
};

/*
 * Sets up the UART on interrupt line `irq` and registers its eventfd with the VM. Returns 0 or
 * -errno.
 */
int zv_uart_init(struct zv_uart *uart, int input_fd, int output_fd, uint32_t irq, struct zv_vm *vm);

void zv_uart_deinit(struct zv_uart *uart);

/*
 * Called when `input_fd` is readable: moves the available input into the receive queue. Returns
 * 0 or -errno. Aborts if the queue overflows, as the Zig version does.
 */
int zv_uart_handle_event(struct zv_uart *uart);

/*
 * Guest access to register `offset` (0-7). Return 0, or -EINVAL (after logging an error) for a
 * write to a register that can't be written.
 */
int zv_uart_write_reg(struct zv_uart *uart, unsigned offset, uint8_t data);
int zv_uart_read_reg(struct zv_uart *uart, unsigned offset, uint8_t *data);

#endif
