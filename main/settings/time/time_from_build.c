#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>          // <--- ОБЯЗАТЕЛЬНО для struct timeval и settimeofday
#include "esp_log.h"
#include "esp_err.h"
#include "time_from_build.h"

static const char *TAG = "TIME_BUILD";
static bool s_time_initialized = false;

// Таблица месяцев для парсинга
static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// Парсинг строки __DATE__ (формат: "MMM DD YYYY" или "MMM  D YYYY")
static int parse_date(const char *date_str, struct tm *tm)
{
    char month_str[4];
    int day, year;
    
    // Копируем месяц (первые 3 символа)
    strncpy(month_str, date_str, 3);
    month_str[3] = '\0';
    
    // Парсим день (может быть " 1" или "15")
    day = atoi(date_str + 4);
    
    // Парсим год
    year = atoi(date_str + 7);
    
    // Находим номер месяца (0-11)
    int month = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) {
            month = i;
            break;
        }
    }
    
    if (month == -1) {
        ESP_LOGE(TAG, "Не удалось распарсить месяц: %s", month_str);
        return ESP_FAIL;
    }
    
    tm->tm_mon = month;
    tm->tm_mday = day;
    tm->tm_year = year - 1900;  // годы от 1900
    
    return ESP_OK;
}

// Парсинг строки __TIME__ (формат: "HH:MM:SS")
static int parse_time(const char *time_str, struct tm *tm)
{
    int hour, min, sec;
    if (sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) != 3) {
        ESP_LOGE(TAG, "Не удалось распарсить время: %s", time_str);
        return ESP_FAIL;
    }
    
    tm->tm_hour = hour;
    tm->tm_min = min;
    tm->tm_sec = sec;
    
    return ESP_OK;
}

esp_err_t time_init_from_build(void)
{
    struct tm timeinfo = {0};
    
    // Получаем дату и время компиляции
    const char *build_date = __DATE__;
    const char *build_time = __TIME__;
    
    ESP_LOGI(TAG, "Время компиляции: %s %s", build_date, build_time);
    
    // Парсим дату
    if (parse_date(build_date, &timeinfo) != ESP_OK) {
        return ESP_FAIL;
    }
    
    // Парсим время
    if (parse_time(build_time, &timeinfo) != ESP_OK) {
        return ESP_FAIL;
    }
    
    // Устанавливаем временную зону (UTC)
    setenv("TZ", "UTC-0", 1);
    tzset();
    
    // Преобразуем struct tm в time_t
    time_t timestamp = mktime(&timeinfo);
    if (timestamp == -1) {
        ESP_LOGE(TAG, "Ошибка mktime");
        return ESP_FAIL;
    }
    
    // Устанавливаем системное время
    struct timeval tv = {
        .tv_sec = timestamp,
        .tv_usec = 0
    };
    
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGE(TAG, "Ошибка settimeofday");
        return ESP_FAIL;
    }
    
    s_time_initialized = true;
    
    // Выводим установленное время для проверки
    char time_str[64];
    time_get_str(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "Системное время установлено: %s", time_str);
    
    return ESP_OK;
}

void time_get_str(char *buffer, size_t max_len)
{
    if (!s_time_initialized) {
        snprintf(buffer, max_len, "Время не инициализировано");
        return;
    }
    
    time_t now = time_get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

time_t time_get_timestamp(void)
{
    time_t now;
    time(&now);
    return now;
}