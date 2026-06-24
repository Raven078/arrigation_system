#include "backlight_manager.h"
#include "settings/display/backlight_ch422g.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BACKLIGHT_MGR";

// Таймер для проверки бездействия
static esp_timer_handle_t s_timer_handle = NULL;
static uint32_t s_last_activity_time = 0;
static bool s_backlight_on = true;
static uint32_t s_timeout_ms = CONFIG_BACKLIGHT_TIMEOUT_SEC * 1000;

// ------------------------------------------------------------------
// Обработчик таймера — проверяет бездействие и управляет подсветкой
// ------------------------------------------------------------------
static void timer_callback(void *arg)
{
    uint32_t now = esp_timer_get_time() / 1000; // ms

    if (s_backlight_on && (now - s_last_activity_time > s_timeout_ms)) {
        // Выключаем подсветку
        backlight_ch422g_set(false);
        s_backlight_on = false;
        ESP_LOGI(TAG, "Backlight OFF (timeout %d sec)", CONFIG_BACKLIGHT_TIMEOUT_SEC);
    }
    // Если подсветка выключена, ничего не делаем — включится при следующем касании
}

// ------------------------------------------------------------------
// Функция сброса таймера бездействия (вызывается при касании)
// ------------------------------------------------------------------
static void reset_activity_timer(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    s_last_activity_time = now;

    // Если подсветка выключена — включаем
    if (!s_backlight_on) {
        backlight_ch422g_set(true);
        s_backlight_on = true;
        ESP_LOGI(TAG, "Backlight ON (touch detected)");
    }
}

// ------------------------------------------------------------------
// Обработчик событий LVGL
// ------------------------------------------------------------------
static void lv_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        reset_activity_timer();
    }
}

// ------------------------------------------------------------------
// Инициализация менеджера
// ------------------------------------------------------------------
esp_err_t backlight_manager_init(void)
{
    // Инициализируем время последней активности сейчас
    s_last_activity_time = esp_timer_get_time() / 1000;
    s_backlight_on = true;

    // Создаём таймер с периодом 100 мс для проверки бездействия
    const esp_timer_create_args_t timer_args = {
        .callback = timer_callback,
        .name = "backlight_timer",
    };
    esp_err_t err = esp_timer_create(&timer_args, &s_timer_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %d", err);
        return err;
    }
    esp_timer_start_periodic(s_timer_handle, 100 * 1000); // 100 ms

    // Подписываемся на события касания на активном экране
    // Подписываемся на все объекты (корневой экран)
    lv_obj_t *scr = lv_scr_act(); // текущий активный экран
    if (scr) {
        lv_obj_add_event_cb(scr, lv_event_cb, LV_EVENT_ALL, NULL);
        ESP_LOGI(TAG, "Subscribed to touch events on screen");
    } else {
        ESP_LOGW(TAG, "No active screen, touch events not subscribed");
    }

    ESP_LOGI(TAG, "Backlight manager initialized (timeout = %d sec)", CONFIG_BACKLIGHT_TIMEOUT_SEC);
    return ESP_OK;
}