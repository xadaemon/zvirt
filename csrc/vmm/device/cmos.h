/*
 * CMOS. Port of vmm/device/cmos.zig.
 *
 * Two I/O ports: the guest writes a register number to 0x70, then reads or writes the register
 * through 0x71. Only the RTC registers are implemented.
 */
#ifndef ZV_VMM_DEVICE_CMOS_H
#define ZV_VMM_DEVICE_CMOS_H

#include "vmm/device/mc146818rtc.h"

#include <stdint.h>

/* Offsets from port 0x70. */
enum zv_cmos_register {
    /* Port 0x70: register select. */
    ZV_CMOS_INDEX = 0,
    /* Port 0x71: data. */
    ZV_CMOS_DATA = 1,
};

struct zv_cmos {
    uint16_t reg_select;
    struct zv_rtc rtc;
};

void zv_cmos_init(struct zv_cmos *cmos);

/* Returns 0, or -EINVAL for a data write to a register the CMOS doesn't have. */
int zv_cmos_write_reg(struct zv_cmos *cmos, enum zv_cmos_register reg, uint8_t data);

/*
 * Stores the register value in *data. Returns 0, or -EINVAL (after logging an error) for a data
 * read from a register the CMOS doesn't have.
 */
int zv_cmos_read_reg(struct zv_cmos *cmos, enum zv_cmos_register reg, uint8_t *data);

#endif
