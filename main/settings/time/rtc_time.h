#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void rtc_time_init(void);
void rtc_time_get_str(char *buffer, size_t max_len);
time_t rtc_time_get_timestamp(void);
void rtc_time_sync_from_ntp(void);   // всегда объявлена

#ifdef __cplusplus
}
#endif

#endif /* RTC_TIME_H */