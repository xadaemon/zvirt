/*
 * x86 real-time clock (MC146818). Port of vmm/device/mc146818rtc.zig.
 *
 * A minimal model: the time registers just store what the guest writes, and the status registers
 * start in 24-hour binary mode. Accessed through the CMOS ports.
 */
#ifndef ZV_VMM_DEVICE_MC146818RTC_H
#define ZV_VMM_DEVICE_MC146818RTC_H

#include <stdbool.h>
#include <stdint.h>

enum zv_rtc_register {
    ZV_RTC_SECONDS = 0,
    ZV_RTC_SECONDS_ALARM = 1,
    ZV_RTC_MINUTES = 2,
    ZV_RTC_MINUTES_ALARM = 3,
    ZV_RTC_HOURS = 4,
    ZV_RTC_HOURS_ALARM = 5,
    ZV_RTC_DAY_WEEK = 6,
    ZV_RTC_DAY_MONTH = 7,
    ZV_RTC_MONTH = 8,
    ZV_RTC_YEAR = 9,
    ZV_RTC_STATUS_A = 10,
    ZV_RTC_STATUS_B = 11,
    ZV_RTC_STATUS_C = 12,
};

struct zv_rtc {
    /* Seconds to year. */
    uint8_t regs[ZV_RTC_YEAR + 1];
    uint8_t status_a;
    uint8_t status_b;
    uint8_t status_c;
};

void zv_rtc_init(struct zv_rtc *rtc);

/* True if `index` names one of the registers above. */
bool zv_rtc_is_register(unsigned index);

void zv_rtc_write_reg(struct zv_rtc *rtc, enum zv_rtc_register reg, uint8_t data);
uint8_t zv_rtc_read_reg(const struct zv_rtc *rtc, enum zv_rtc_register reg);

#endif
