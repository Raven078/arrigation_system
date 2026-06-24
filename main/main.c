#include "settings/display/display_init.h"
#include "settings/touch/touch.h"
#include "settings/wifi/wifi_ap.h"
#include "settings/wifi/tcp_server.h"
#include "settings/time/rtc_time.h"
#include "settings/display/backlight_ch422g.h"
#include "GUI/ui/ui.h"
#include "GUI/wallpaper/wallpaper800400.h"
#include "file_logger.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "lvgl.h"

static const char *TAG = "main";

static void cleanup_task(void *pvParameters)
{
    // Очистка старых логов уже выполняется при старте, здесь только периодическая проверка
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(24 * 60 * 60 * 1000)); // каждые 24 часа
        file_logger_cleanup_old_logs(5);  // удалять файлы старше 5 дней (для безопасности)
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    rtc_time_init();
    ESP_LOGI(TAG, "RTC initialized");

    file_logger_init();
    ESP_LOGI(TAG, "SPIFFS initialized");

    // === УДАЛЕНИЕ ВСЕХ СТАРЫХ ЛОГ-ФАЙЛОВ ПРИ ЗАПУСКЕ ===
    file_logger_cleanup_old_logs(0);
    ESP_LOGI(TAG, "All old log files deleted (age > 0 days)");

    xTaskCreate(cleanup_task, "cleanup_logs", 4096, NULL, 1, NULL);
    ESP_LOGI(TAG, "Cleanup task started");

    lv_display_t *disp = display_init();
    if (!disp) {
        ESP_LOGE(TAG, "Display init failed");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Display created");

    touch_init(disp);
    ESP_LOGI(TAG, "Touch initialized (bus)");

    ret = backlight_ch422g_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CH422G init failed, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    backlight_ch422g_set(true);
    ESP_LOGI(TAG, "Backlight enabled (always on)");

    touch_reinit();
    ESP_LOGI(TAG, "Touch re-initialized after backlight init");

    vTaskDelay(pdMS_TO_TICKS(200));

    ui_create_wallpaper();
    ESP_LOGI(TAG, "UI created");

    wifi_init_softap();
    ESP_LOGI(TAG, "Wi-Fi AP started");

    tcp_server_start(8888);
    ESP_LOGI(TAG, "TCP server started");

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}