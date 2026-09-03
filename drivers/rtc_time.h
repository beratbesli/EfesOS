#ifndef EFESOS_RTC_TIME_H
#define EFESOS_RTC_TIME_H

#include "rtc.h"

#define RTC_REGISTER_B_24_HOUR 0x02U
#define RTC_REGISTER_B_BINARY 0x04U

struct rtc_raw_snapshot {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned char year;
    unsigned char century;
    unsigned char register_b;
};

int rtc_raw_snapshots_equal(const struct rtc_raw_snapshot *left,
    const struct rtc_raw_snapshot *right);
int rtc_decode_snapshot(const struct rtc_raw_snapshot *raw,
    struct rtc_time *time);
int rtc_time_is_valid(const struct rtc_time *time);

#endif
