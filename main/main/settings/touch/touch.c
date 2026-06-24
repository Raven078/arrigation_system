#include "touch.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"
#include "lvgl.h"
#include "settings/display/backlight_ch422g.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

static esp_lcd_touch_handle_t tp = NULL;
static lv_indev_t *s_indev = NULL;

i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

// Колбэк чтения для LVGL (простой, без выключения подсветки)
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (!tp) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_err_t err = esp_lcd_touch_read_data(tp);
    if (err != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_lcd_touch_point_data_t points[1];
    uint8_t touch_cnt = 0;
    err = esp_lcd_touch_get_data(tp, points, &touch_cnt, 1);
    if (err != ESP_OK || touch_cnt == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = points[0].x;
    data->point.y = points[0].y;
}

void touch_reinit(void)
{
    if (g_i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized! Call touch_init first.");
        return;
    }

    ESP_LOGI(TAG, "Re-initializing GT911");

    if (tp) {
        // esp_lcd_touch_del(tp);
        tp = NULL;
    }

    // Подсветка всегда включена, поэтому сброс делаем сразу
    backlight_ch422g_touch_reset();

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    esp_lcd_panel_io_handle_t io_handle;
    esp_err_t err = esp_lcd_new_panel_io_i2c(g_i2c_bus_handle, &io_config, &io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C IO");
        return;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,
        .y_max = 480,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    err = esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &tp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GT911 driver");
        return;
    }

    if (s_indev == NULL) {
        s_indev = lv_indev_create();
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    }
    lv_indev_set_read_cb(s_indev, touchpad_read);

    ESP_LOGI(TAG, "GT911 re-initialized successfully");
}

void touch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Init GT911 (bus only)");

    esp_log_level_set("GT911", ESP_LOG_NONE);
    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);

    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = 0,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &g_i2c_bus_handle));

    vTaskDelay(pdMS_TO_TICKS(200));

    esp_err_t probe = i2c_master_probe(g_i2c_bus_handle, 0x5D, pdMS_TO_TICKS(100));
    if (probe != ESP_OK) {
        ESP_LOGE(TAG, "GT911 not found at 0x5D!");
        return;
    }
    ESP_LOGI(TAG, "GT911 detected on I2C");

    touch_reinit();
}