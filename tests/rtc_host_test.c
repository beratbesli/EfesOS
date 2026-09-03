#include <stdio.h>

#include "rtc_time.h"

static int expect_time(const struct rtc_raw_snapshot *raw, unsigned int year,
    unsigned int month, unsigned int day, unsigned int hour,
    unsigned int minute, unsigned int second)
{
    struct rtc_time time;

    return rtc_decode_snapshot(raw, &time) && time.year == year &&
        time.month == month && time.day == day && time.hour == hour &&
        time.minute == minute && time.second == second;
}

int main(void)
{
    struct rtc_raw_snapshot raw = {
        0x56U, 0x34U, 0x12U, 0x29U, 0x02U, 0x24U, 0x20U,
        RTC_REGISTER_B_24_HOUR
    };
    struct rtc_raw_snapshot copy = raw;

    if (!expect_time(&raw, 2024U, 2U, 29U, 12U, 34U, 56U) ||
        !rtc_raw_snapshots_equal(&raw, &copy)) {
        return 1;
    }
    raw.register_b = RTC_REGISTER_B_24_HOUR | RTC_REGISTER_B_BINARY;
    raw.second = 7U;
    raw.minute = 8U;
    raw.hour = 9U;
    raw.day = 3U;
    raw.month = 9U;
    raw.year = 26U;
    raw.century = 20U;
    if (!expect_time(&raw, 2026U, 9U, 3U, 9U, 8U, 7U)) {
        return 1;
    }
    raw.register_b = 0U;
    raw.second = 0U;
    raw.minute = 0U;
    raw.hour = 0x12U;
    raw.day = 0x01U;
    raw.month = 0x01U;
    raw.year = 0x79U;
    raw.century = 0U;
    if (!expect_time(&raw, 2079U, 1U, 1U, 0U, 0U, 0U)) {
        return 1;
    }
    raw.hour = 0x92U;
    raw.year = 0x80U;
    if (!expect_time(&raw, 1980U, 1U, 1U, 12U, 0U, 0U)) {
        return 1;
    }
    raw.year = 0x23U;
    raw.month = 0x02U;
    raw.day = 0x29U;
    if (rtc_decode_snapshot(&raw, &(struct rtc_time){0})) {
        return 1;
    }
    raw.day = 0x28U;
    raw.second = 0x6AU;
    if (rtc_decode_snapshot(&raw, &(struct rtc_time){0})) {
        return 1;
    }
    copy = raw;
    copy.minute++;
    if (rtc_raw_snapshots_equal(&raw, &copy) ||
        rtc_raw_snapshots_equal(0, &copy)) {
        return 1;
    }
    puts("RTC conversion host self-test passed.");
    return 0;
}
