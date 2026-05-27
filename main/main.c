#include "settings/display/display_init.h"
#include "settings/touch/touch.h"
#include "settings/wifi/wifi_ap.h"
#include "settings/wifi/tcp_server.h"
#include "settings/time/rtc_time.h"
#include "GUI/ui/ui.h"
#include "GUI/wallpaper/wallpaper800400.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "esp_heap_caps.h"

void app_main(void) {
    ESP_LOGI("main", "Starting application");

    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("main", "NVS erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // RTC
    rtc_time_init();

    // Дисплей
    lv_display_t *disp = display_init();
    if (!disp) {
        ESP_LOGE("main", "Display init failed");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Тачпад
    touch_init(disp);

    // Создание UI (фон из flash, кнопки)
    ui_create_wallpaper();

    // Запуск Wi-Fi AP и TCP-сервера
    wifi_init_softap();
    tcp_server_start(8888);

    // Главный цикл
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}