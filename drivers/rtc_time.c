#include "rtc_time.h"

static int decode_bcd(unsigned char raw, unsigned int *value)
{
    unsigned int low = raw & 0x0FU;
    unsigned int high = (raw >> 4U) & 0x0FU;

    if (value == 0 || low > 9U || high > 9U) {
        return 0;
    }
    *value = high * 10U + low;
    return 1;
}

static int decode_value(unsigned char raw, int binary,
    unsigned int *value)
{
    if (value == 0) {
        return 0;
    }
    if (binary) {
        *value = raw;
        return 1;
    }
    return decode_bcd(raw, value);
}

static int leap_year(unsigned int year)
{
    return (year % 4U == 0U && year % 100U != 0U) ||
        year % 400U == 0U;
}

static unsigned int month_days(unsigned int year, unsigned int month)
{
    static const unsigned char days[12] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if (month == 0U || month > 12U) {
        return 0U;
    }
    if (month == 2U && leap_year(year)) {
        return 29U;
    }
    return days[month - 1U];
}

int rtc_raw_snapshots_equal(const struct rtc_raw_snapshot *left,
    const struct rtc_raw_snapshot *right)
{
    return left != 0 && right != 0 &&
        left->second == right->second &&
        left->minute == right->minute && left->hour == right->hour &&
        left->day == right->day && left->month == right->month &&
        left->year == right->year && left->century == right->century &&
        left->register_b == right->register_b;
}

int rtc_time_is_valid(const struct rtc_time *time)
{
    unsigned int days;

    if (time == 0 || time->year < 1900U || time->year > 9999U ||
        time->month == 0U || time->month > 12U ||
        time->hour > 23U || time->minute > 59U || time->second > 59U) {
        return 0;
    }
    days = month_days(time->year, time->month);
    return time->day != 0U && time->day <= days;
}

int rtc_decode_snapshot(const struct rtc_raw_snapshot *raw,
    struct rtc_time *time)
{
    unsigned int second;
    unsigned int minute;
    unsigned int hour;
    unsigned int day;
    unsigned int month;
    unsigned int year;
    unsigned int century = 0U;
    unsigned char hour_raw;
    int binary;
    int hour_24;
    int afternoon;

    if (raw == 0 || time == 0) {
        return 0;
    }
    binary = (raw->register_b & RTC_REGISTER_B_BINARY) != 0U;
    hour_24 = (raw->register_b & RTC_REGISTER_B_24_HOUR) != 0U;
    afternoon = !hour_24 && (raw->hour & 0x80U) != 0U;
    hour_raw = hour_24 ? raw->hour : (unsigned char)(raw->hour & 0x7FU);

    if (!decode_value(raw->second, binary, &second) ||
        !decode_value(raw->minute, binary, &minute) ||
        !decode_value(hour_raw, binary, &hour) ||
        !decode_value(raw->day, binary, &day) ||
        !decode_value(raw->month, binary, &month) ||
        !decode_value(raw->year, binary, &year)) {
        return 0;
    }
    if (!hour_24) {
        if (hour == 0U || hour > 12U) {
            return 0;
        }
        if (hour == 12U) {
            hour = 0U;
        }
        if (afternoon) {
            hour += 12U;
        }
    }

    if (raw->century != 0U &&
        decode_value(raw->century, binary, &century) &&
        century >= 19U && century <= 99U) {
        year += century * 100U;
    } else {
        /* The conventional PC century byte is optional. Use the same
           bounded 1980-2079 pivot as firmware time APIs when it is absent. */
        year += year >= 80U ? 1900U : 2000U;
    }

    time->year = year;
    time->month = (unsigned char)month;
    time->day = (unsigned char)day;
    time->hour = (unsigned char)hour;
    time->minute = (unsigned char)minute;
    time->second = (unsigned char)second;
    return rtc_time_is_valid(time);
}
