#include "rtc_time.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"
#include "sys/time.h"

static const char *TAG = "RTC_TIME";
static volatile time_t s_current_timestamp = 0;
static volatile bool s_time_initialized = false;

// Таблица месяцев для парсинга __DATE__
static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// Парсинг строки __DATE__ (формат: "MMM DD YYYY")
static int parse_date(const char *date_str, struct tm *tm)
{
    char month_str[4];
    int day, year;
    
    strncpy(month_str, date_str, 3);
    month_str[3] = '\0';
    day = atoi(date_str + 4);
    year = atoi(date_str + 7);
    
    int month = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) {
            month = i;
            break;
        }
    }
    
    if (month == -1) return ESP_FAIL;
    
    tm->tm_mon = month;
    tm->tm_mday = day;
    tm->tm_year = year - 1900;
    
    return ESP_OK;
}

// Парсинг строки __TIME__ (формат: "HH:MM:SS")
static int parse_time(const char *time_str, struct tm *tm)
{
    int hour, min, sec;
    if (sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) != 3) {
        return ESP_FAIL;
    }
    
    tm->tm_hour = hour;
    tm->tm_min = min;
    tm->tm_sec = sec;
    
    return ESP_OK;
}

// Задача обновления времени
static void rtc_update_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
        
        if (s_time_initialized) {
            s_current_timestamp++;
        }
    }
}

void rtc_time_init(void)
{
    struct tm timeinfo = {0};
    
    // Парсим дату и время компиляции
    if (parse_date(__DATE__, &timeinfo) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse __DATE__");
        return;
    }
    
    if (parse_time(__TIME__, &timeinfo) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse __TIME__");
        return;
    }
    
    // Устанавливаем временную зону (UTC+3 для Москвы)
    setenv("TZ", "MSK-3", 1);
    tzset();
    
    // Устанавливаем системное время
    time_t timestamp = mktime(&timeinfo);
    struct timeval tv = {
        .tv_sec = timestamp,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
    
    s_current_timestamp = timestamp;
    s_time_initialized = true;
    
    char time_str[64];
    rtc_time_get_str(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "RTC время установлено: %s", time_str);
    
    // Запускаем задачу обновления времени
    xTaskCreate(rtc_update_task, "rtc_update", 2048, NULL, 5, NULL);
}

void rtc_time_get_str(char *buffer, size_t max_len)
{
    if (!s_time_initialized) {
        snprintf(buffer, max_len, "Time not initialized");
        return;
    }
    
    time_t now = rtc_time_get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

time_t rtc_time_get_timestamp(void)
{
    if (!s_time_initialized) {
        return 0;
    }
    return s_current_timestamp;
}