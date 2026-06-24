#include "rtc_time.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "sys/time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include "sdkconfig.h"

static const char *TAG = "RTC_TIME";
static time_t s_current_timestamp = 0;
static bool s_time_initialized = false;
static bool ntp_initialized = false;

void rtc_time_init(void)
{
    // Парсим __DATE__: "MMM DD YYYY"
    const char* date = __DATE__;
    const char* time_str = __TIME__;
    char month_str[4];
    int day, year;
    sscanf(date, "%3s %d %d", month_str, &day, &year);
    // Преобразуем месяц в номер (0-11)
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    int month = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(months[i], month_str) == 0) {
            month = i;
            break;
        }
    }
    if (month == -1) {
        ESP_LOGE(TAG, "Failed to parse month from __DATE__");
        return;
    }

    struct tm tm = {0};
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_year = year - 1900;
    // Парсим __TIME__: "HH:MM:SS"
    sscanf(time_str, "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_current_timestamp = t;
    s_time_initialized = true;
    char buf[64];
    rtc_time_get_str(buf, sizeof(buf));
    ESP_LOGI(TAG, "RTC время установлено: %s", buf);
}

void rtc_time_get_str(char *buffer, size_t max_len)
{
    if (!s_time_initialized) {
        snprintf(buffer, max_len, "Time not initialized");
        return;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", &tm);
}

time_t rtc_time_get_timestamp(void)
{
    return s_time_initialized ? time(NULL) : 0;
}

#ifdef CONFIG_WIFI_STA_ENABLE
void rtc_time_sync_from_ntp(void)
{
    if (ntp_initialized) {
        ESP_LOGW(TAG, "NTP already initialized, skipping");
        return;
    }

    ESP_LOGI(TAG, "Starting NTP sync from %s", CONFIG_NTP_SERVER);

    char tz_str[16];
    snprintf(tz_str, sizeof(tz_str), "UTC%+d", -CONFIG_TIMEZONE_OFFSET);
    setenv("TZ", tz_str, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to UTC%+d", CONFIG_TIMEZONE_OFFSET);

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_NTP_SERVER);
    esp_sntp_init();
    ntp_initialized = true;

    int retry = 0;
    const int retry_count = 20;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (retry == retry_count) {
        ESP_LOGW(TAG, "NTP sync timeout");
        return;
    }

    time_t now = time(NULL);
    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_current_timestamp = now;
    s_time_initialized = true;

    char time_str[64];
    rtc_time_get_str(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "NTP time synchronized: %s", time_str);
}
#endif