/*
 * Virtual 16550 UART (serial port). Port of vmm/device/uart_16550.zig.
 *
 * Deviation: writing register 6 (the read-only modem status register), or register 5 without
 * DLAB, panics or fails in Zig. The guest controls this, so here both log an error and return
 * -EINVAL, which stops the vCPU.
 */
#include "vmm/device/uart_16550.h"

#include "utils/log.h"
#include "vmm/vmm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LOG_SCOPE "uart_16550"

/* Line control register: bits 0-1 word length (0b11 = 8 bits), bit 7 divisor latch access. */
#define LCR_DEFAULT 0x03
#define LCR_DLAB (1 << 7)

/* Line status register. */
#define LSR_DATA_READY (1 << 0)
/* Transmit holding register empty, transmitter empty: always set, output is synchronous. */
#define LSR_THRE (1 << 5)
#define LSR_TEMT (1 << 6)

/* Interrupt enable register. */
#define IER_RECEIVE_DATA (1 << 0)
#define IER_THRE (1 << 1)
#define IER_RECEIVE_LINE (1 << 2)

/*
 * Interrupt identification register: bit 0 set means "no interrupt pending", bits 1-2 say which
 * interrupt is pending. Priority: receive line > receive data > transmitter empty.
 */
#define IIR_NOT_PENDING (1 << 0)
#define IIR_IRQ_SHIFT 1
#define IIR_IRQ_MASK (0x3 << IIR_IRQ_SHIFT)

enum uart_irq {
    IRQ_NONE = 0x0,
    IRQ_TRANSMITTER_EMPTY = 0x1,
    IRQ_RECEIVE_DATA = 0x2,
    IRQ_RECEIVE_LINE = 0x3,
};

/* Modem status register value: DCD, DSR and CTS set (a cable is attached). */
#define MSR_VALUE 0xb0

/* Register offsets. Offsets 0 and 1 change meaning when LCR_DLAB is set. */
enum uart_offset {
    OFFSET_DATA = 0,    /* RBR (read) / THR (write), or DLL with DLAB */
    OFFSET_IER = 1,     /* IER, or DLH with DLAB */
    OFFSET_IIR_FCR = 2, /* IIR (read) / FCR (write) */
    OFFSET_LCR = 3,
    OFFSET_MCR = 4,
    OFFSET_LSR = 5,
    OFFSET_MSR = 6,
    OFFSET_SCR = 7,
};

#define INPUT_CHUNK 1024

int zv_uart_init(struct zv_uart *uart, int input_fd, int output_fd, uint32_t irq, struct zv_vm *vm)
{
    uart->input_fd = input_fd;
    uart->output_fd = output_fd;
    uart->lcr = LCR_DEFAULT;
    uart->scr = 0;
    uart->mcr = 0;
    uart->ier = 0;
    uart->dll = 1;
    uart->dlh = 0;
    uart->lsr = LSR_THRE | LSR_TEMT;
    uart->iir = IIR_NOT_PENDING;
    uart->thre_pending = false;
    uart->rx_head = 0;
    uart->rx_count = 0;
    uart->irq = irq;
    uart->irq_set = false;

    int err = zv_eventfd_new(0, &uart->irqfd);

    if (err < 0)
        return err;

    err = zv_vm_register_irq(vm, &uart->irqfd, irq);
    if (err < 0) {
        zv_eventfd_deinit(&uart->irqfd);
        return err;
    }

    return 0;
}

void zv_uart_deinit(struct zv_uart *uart)
{
    zv_eventfd_deinit(&uart->irqfd);
}

static bool dlab(const struct zv_uart *uart)
{
    return (uart->lcr & LCR_DLAB) != 0;
}

static bool irq_enabled(const struct zv_uart *uart)
{
    return (uart->ier & (IER_THRE | IER_RECEIVE_LINE | IER_RECEIVE_DATA)) != 0;
}

static int set_irq(struct zv_uart *uart, enum uart_irq irq)
{
    if (!irq_enabled(uart))
        return 0;

    uart->iir = (uint8_t)(irq << IIR_IRQ_SHIFT);

    if (!uart->irq_set) {
        int err = zv_eventfd_notify(&uart->irqfd);

        if (err < 0)
            return err;

        uart->irq_set = true;
    }

    return 0;
}

static void clear_irq(struct zv_uart *uart)
{
    uart->iir = IIR_NOT_PENDING;
    uart->irq_set = false;
}

/* Raises the highest-priority pending interrupt, or clears the interrupt if none is pending. */
static int update_irq(struct zv_uart *uart)
{
    if (uart->lsr & LSR_DATA_READY)
        return set_irq(uart, IRQ_RECEIVE_DATA);

    if (uart->thre_pending)
        return set_irq(uart, IRQ_TRANSMITTER_EMPTY);

    clear_irq(uart);
    return 0;
}

static int push_rx_bytes(struct zv_uart *uart, const uint8_t *bytes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (uart->rx_count == ZV_UART_RX_CAPACITY) {
            fprintf(stderr, "uart: receive queue overflow\n");
            abort();
        }

        size_t tail = (uart->rx_head + uart->rx_count) % ZV_UART_RX_CAPACITY;

        uart->rx_storage[tail] = bytes[i];
        uart->rx_count++;
    }

    if (uart->rx_count > 0) {
        uart->lsr |= LSR_DATA_READY;
        return update_irq(uart);
    }

    return 0;
}

/* Returns the next received byte, or 0 if the queue is empty. */
static uint8_t pop_rx_byte(struct zv_uart *uart)
{
    uint8_t byte = 0;

    if (uart->rx_count > 0) {
        byte = uart->rx_storage[uart->rx_head];
        uart->rx_head = (uart->rx_head + 1) % ZV_UART_RX_CAPACITY;
        uart->rx_count--;
    }

    if (uart->rx_count == 0)
        uart->lsr &= (uint8_t)~LSR_DATA_READY;

    return byte;
}

int zv_uart_handle_event(struct zv_uart *uart)
{
    /* Read in chunks until a short read says there is nothing more for now. */
    while (true) {
        uint8_t buffer[INPUT_CHUNK];
        ssize_t bytes_read = read(uart->input_fd, buffer, sizeof(buffer));

        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read < 0)
            return -errno;

        int err = push_rx_bytes(uart, buffer, (size_t)bytes_read);

        if (err < 0)
            return err;

        if (bytes_read < INPUT_CHUNK)
            return 0;
    }
}

static int write_byte(struct zv_uart *uart, uint8_t byte)
{
    while (true) {
        ssize_t written = write(uart->output_fd, &byte, 1);

        if (written == 1)
            return 0;
        if (written < 0 && errno != EINTR)
            return -errno;
    }
}

int zv_uart_write_reg(struct zv_uart *uart, unsigned offset, uint8_t data)
{
    int err;

    switch (offset) {
    case OFFSET_DATA:
        if (dlab(uart)) {
            uart->dll = data;
            return 0;
        }

        /* Transmit holding register: the byte goes straight out. */
        err = write_byte(uart, data);
        if (err < 0)
            return err;

        uart->thre_pending = true;
        return update_irq(uart);
    case OFFSET_IER:
        if (dlab(uart)) {
            uart->dlh = data;
            return 0;
        }

        uart->ier = data;
        return update_irq(uart);
    case OFFSET_IIR_FCR:
        /* FIFO control: FIFOs aren't modelled. */
        return 0;
    case OFFSET_LCR:
        uart->lcr = data;
        return 0;
    case OFFSET_MCR:
        uart->mcr = data;
        return 0;
    case OFFSET_LSR:
        /* Line status is read-only; Zig accepts (and ignores) writes only while DLAB is set. */
        if (dlab(uart))
            return 0;

        zv_log_err(LOG_SCOPE, "write to line status register");
        return -EINVAL;
    case OFFSET_SCR:
        uart->scr = data;
        return 0;
    default:
        zv_log_err(LOG_SCOPE, "write to read-only register %u", offset);
        return -EINVAL;
    }
}

int zv_uart_read_reg(struct zv_uart *uart, unsigned offset, uint8_t *data)
{
    switch (offset) {
    case OFFSET_DATA:
        if (dlab(uart)) {
            *data = uart->dll;
            return 0;
        }

        /* Receive buffer register. */
        *data = pop_rx_byte(uart);
        return update_irq(uart);
    case OFFSET_IER:
        *data = dlab(uart) ? uart->dlh : uart->ier;
        return 0;
    case OFFSET_IIR_FCR: {
        *data = uart->iir;

        /* Reading IIR acknowledges a pending "transmitter empty" interrupt. */
        unsigned pending_irq = (uart->iir & IIR_IRQ_MASK) >> IIR_IRQ_SHIFT;

        if (pending_irq == IRQ_TRANSMITTER_EMPTY) {
            if (!uart->thre_pending) {
                fprintf(stderr, "uart: transmitter-empty interrupt without a pending THRE\n");
                abort();
            }

            uart->thre_pending = false;
            return update_irq(uart);
        }

        return 0;
    }
    case OFFSET_LCR:
        *data = uart->lcr;
        return 0;
    case OFFSET_MCR:
        *data = uart->mcr;
        return 0;
    case OFFSET_LSR:
        *data = uart->lsr;
        return 0;
    case OFFSET_MSR:
        *data = MSR_VALUE;
        return 0;
    case OFFSET_SCR:
        *data = uart->scr;
        return 0;
    default:
        zv_log_err(LOG_SCOPE, "read of invalid register %u", offset);
        return -EINVAL;
    }
}
