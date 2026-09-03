#ifndef EFESOS_RTC_H
#define EFESOS_RTC_H

struct rtc_time {
    unsigned int year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
};

int rtc_init(void);
int rtc_available(void);
int rtc_read_time(struct rtc_time *time);
int rtc_self_test(void);

#endif
