#include "touch.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "touch";
static esp_lcd_touch_handle_t tp = NULL;

void touch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Init GT911 touch controller");

    // 1. Настройка I2C шины (пины для Waveshare ESP32-S3 Touch LCD-7)
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = 0,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    i2c_master_bus_handle_t i2c_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));

    // 2. Настройка I2C IO для GT911
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    esp_lcd_panel_io_handle_t io_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &io_config, &io_handle));

    // 3. Конфигурация самого тачпада
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,
        .y_max = 480,
        .rst_gpio_num = GPIO_NUM_NC,   // сброс не используется
        .int_gpio_num = GPIO_NUM_NC,   // прерывание не используется
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &tp));

    // 4. Регистрация touch-драйвера в LVGL через esp_lvgl_port (ЭТО КЛЮЧЕВОЙ МОМЕНТ!)
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,          // передаём указатель на дисплей LVGL
        .handle = tp,          // handle тачпада
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "GT911 initialized and registered with LVGL");
}