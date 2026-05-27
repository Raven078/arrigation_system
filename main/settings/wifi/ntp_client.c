#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "ntp_client.h"

static const char *TAG = "NTP_CLIENT";
static bool s_time_synced = false;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Время успешно синхронизировано");
    s_time_synced = true;
}

esp_err_t ntp_sync_init(void)
{
    // 1. Настройка SNTP (классический API)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_NTP_SERVER);
    esp_sntp_init();

    // 2. Попробуем установить коллбек (если доступен)
    // В некоторых версиях IDF функция называется sntp_set_time_sync_notification_cb
    // Используем оба варианта
    #ifdef CONFIG_SNTP_SYNC_CALLBACK
    esp_sntp_set_sync_cb(time_sync_notification_cb);
    #else
    // Альтернативный способ: просто будем ждать
    ESP_LOGI(TAG, "SNTP инициализирован, ожидание синхронизации...");
    #endif

    // 3. Установка часового пояса
    setenv("TZ", CONFIG_NTP_TIMEZONE, 1);
    tzset();

    // 4. Ожидаем, пока время не станет осмысленным (год > 2016)
    int retry = 0;
    const int max_retry = 15;
    while (retry < max_retry) {
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2016 - 1900)) { // 2016 год
            s_time_synced = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }

    if (s_time_synced) {
        char time_str[64];
        ntp_get_time_str(time_str, sizeof(time_str));
        ESP_LOGI(TAG, "Текущее время: %s", time_str);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Не удалось синхронизировать время через %d сек", max_retry);
        return ESP_FAIL;
    }
}

void ntp_get_time_str(char *buffer, size_t max_len)
{
    time_t now = ntp_get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

time_t ntp_get_timestamp(void)
{
    time_t now;
    time(&now);
    return now;
}