#include "io.h"
#include "rtc.h"
#include "rtc_time.h"

#define CMOS_INDEX_PORT 0x70U
#define CMOS_DATA_PORT 0x71U
#define CMOS_NMI_DISABLE 0x80U
#define RTC_SECONDS 0x00U
#define RTC_MINUTES 0x02U
#define RTC_HOURS 0x04U
#define RTC_DAY 0x07U
#define RTC_MONTH 0x08U
#define RTC_YEAR 0x09U
#define RTC_REGISTER_A 0x0AU
#define RTC_REGISTER_B 0x0BU
#define RTC_REGISTER_D 0x0DU
#define RTC_CENTURY 0x32U
#define RTC_UPDATE_IN_PROGRESS 0x80U
#define RTC_VALID_RAM_AND_TIME 0x80U
#define RTC_UIP_WAIT_LIMIT 100000U
#define RTC_STABLE_READ_ATTEMPTS 8U

static int available;

static unsigned int rtc_irq_save(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void rtc_irq_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

static unsigned char cmos_read(unsigned char index, int nmi_disabled)
{
    outb(CMOS_INDEX_PORT, (unsigned char)(index |
        (nmi_disabled ? CMOS_NMI_DISABLE : 0U)));
    return inb(CMOS_DATA_PORT);
}

static int wait_for_update(void)
{
    unsigned int attempt;

    for (attempt = 0U; attempt < RTC_UIP_WAIT_LIMIT; attempt++) {
        if ((cmos_read(RTC_REGISTER_A, 0) & RTC_UPDATE_IN_PROGRESS) == 0U) {
            return 1;
        }
        __asm__ volatile ("pause" : : : "memory");
    }
    return 0;
}

static int read_raw_snapshot(struct rtc_raw_snapshot *raw)
{
    unsigned int flags;
    unsigned char register_a_before;
    unsigned char register_a_after;
    unsigned char register_d;

    if (raw == 0 || !wait_for_update()) {
        return 0;
    }
    flags = rtc_irq_save();
    register_a_before = cmos_read(RTC_REGISTER_A, 1);
    raw->second = cmos_read(RTC_SECONDS, 1);
    raw->minute = cmos_read(RTC_MINUTES, 1);
    raw->hour = cmos_read(RTC_HOURS, 1);
    raw->day = cmos_read(RTC_DAY, 1);
    raw->month = cmos_read(RTC_MONTH, 1);
    raw->year = cmos_read(RTC_YEAR, 1);
    raw->century = cmos_read(RTC_CENTURY, 1);
    raw->register_b = cmos_read(RTC_REGISTER_B, 1);
    register_d = cmos_read(RTC_REGISTER_D, 1);
    register_a_after = cmos_read(RTC_REGISTER_A, 1);
    /* Restore the index port with NMI delivery enabled before restoring IF. */
    outb(CMOS_INDEX_PORT, 0U);
    rtc_irq_restore(flags);

    return (register_a_before & RTC_UPDATE_IN_PROGRESS) == 0U &&
        (register_a_after & RTC_UPDATE_IN_PROGRESS) == 0U &&
        (register_d & RTC_VALID_RAM_AND_TIME) != 0U;
}

int rtc_read_time(struct rtc_time *time)
{
    struct rtc_raw_snapshot first;
    struct rtc_raw_snapshot second;
    unsigned int attempt;

    if (time == 0) {
        return 0;
    }
    for (attempt = 0U; attempt < RTC_STABLE_READ_ATTEMPTS; attempt++) {
        if (read_raw_snapshot(&first) && read_raw_snapshot(&second) &&
            rtc_raw_snapshots_equal(&first, &second) &&
            rtc_decode_snapshot(&second, time)) {
            return 1;
        }
    }
    available = 0;
    return 0;
}

int rtc_init(void)
{
    struct rtc_time time;

    available = rtc_read_time(&time);
    return available;
}

int rtc_available(void)
{
    return available;
}

int rtc_self_test(void)
{
    struct rtc_raw_snapshot leap = {
        0x59U, 0x58U, 0x23U, 0x29U, 0x02U, 0x24U, 0x20U,
        RTC_REGISTER_B_24_HOUR
    };
    struct rtc_time time;

    if (!rtc_decode_snapshot(&leap, &time) || time.year != 2024U ||
        time.month != 2U || time.day != 29U || time.hour != 23U ||
        time.minute != 58U || time.second != 59U) {
        return 0;
    }
    leap.day = 0x30U;
    return !rtc_decode_snapshot(&leap, &time);
}
