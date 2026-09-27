/*
 * x86 real-time clock (MC146818). Port of vmm/device/mc146818rtc.zig.
 */
#include "vmm/device/mc146818rtc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Status A: rate select 0b0110 (bits 0-3), divider 0b010 (bits 4-6), no update in progress. */
#define STATUS_A_DEFAULT ((0x2 << 4) | 0x6)

/* Status B: bit 1 = 24-hour mode, bit 2 = binary (not BCD) values. */
#define STATUS_B_DEFAULT ((1 << 1) | (1 << 2))

void zv_rtc_init(struct zv_rtc *rtc)
{
    memset(rtc->regs, 0, sizeof(rtc->regs));
    rtc->status_a = STATUS_A_DEFAULT;
    rtc->status_b = STATUS_B_DEFAULT;
    rtc->status_c = 0;
}

bool zv_rtc_is_register(unsigned index)
{
    return index <= ZV_RTC_STATUS_C;
}

void zv_rtc_write_reg(struct zv_rtc *rtc, enum zv_rtc_register reg, uint8_t data)
{
    if (reg <= ZV_RTC_YEAR)
        rtc->regs[reg] = data;
    else if (reg == ZV_RTC_STATUS_A)
        rtc->status_a = data;
    else if (reg == ZV_RTC_STATUS_B)
        rtc->status_b = data;
    else if (reg == ZV_RTC_STATUS_C)
        rtc->status_c = data;
}

uint8_t zv_rtc_read_reg(const struct zv_rtc *rtc, enum zv_rtc_register reg)
{
    if (reg <= ZV_RTC_YEAR)
        return rtc->regs[reg];
    if (reg == ZV_RTC_STATUS_A)
        return rtc->status_a;
    if (reg == ZV_RTC_STATUS_B)
        return rtc->status_b;
    if (reg == ZV_RTC_STATUS_C)
        return rtc->status_c;

    /* The CMOS only passes valid registers (see zv_rtc_is_register). */
    fprintf(stderr, "rtc: read of invalid register %d\n", (int)reg);
    abort();
}
