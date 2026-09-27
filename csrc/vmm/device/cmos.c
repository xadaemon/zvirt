/*
 * CMOS. Port of vmm/device/cmos.zig.
 *
 * Deviation: reading an unsupported register panics in Zig. The guest controls the register
 * number, so here it logs an error and returns -EINVAL, which stops the vCPU.
 */
#include "vmm/device/cmos.h"

#include "utils/log.h"

#include <errno.h>

#define LOG_SCOPE "cmos"

/* Bit 7 of the index port disables NMIs; it isn't part of the register number. */
#define NMI_DISABLE_BIT (1 << 7)

/* The shutdown status register. Writes to it are accepted and ignored. */
#define SHUTDOWN_STATUS_REGISTER 0xf

void zv_cmos_init(struct zv_cmos *cmos)
{
    cmos->reg_select = 0xD;
    zv_rtc_init(&cmos->rtc);
}

int zv_cmos_write_reg(struct zv_cmos *cmos, enum zv_cmos_register reg, uint8_t data)
{
    if (reg == ZV_CMOS_INDEX) {
        cmos->reg_select = data & ~NMI_DISABLE_BIT;
        return 0;
    }

    if (zv_rtc_is_register(cmos->reg_select)) {
        zv_rtc_write_reg(&cmos->rtc, (enum zv_rtc_register)cmos->reg_select, data);
        return 0;
    }

    if (cmos->reg_select == SHUTDOWN_STATUS_REGISTER)
        return 0;

    return -EINVAL;
}

int zv_cmos_read_reg(struct zv_cmos *cmos, enum zv_cmos_register reg, uint8_t *data)
{
    if (reg == ZV_CMOS_INDEX) {
        *data = 0xff;
        return 0;
    }

    if (zv_rtc_is_register(cmos->reg_select)) {
        *data = zv_rtc_read_reg(&cmos->rtc, (enum zv_rtc_register)cmos->reg_select);
        return 0;
    }

    zv_log_err(LOG_SCOPE, "unsupported register %u", (unsigned)cmos->reg_select);
    return -EINVAL;
}
