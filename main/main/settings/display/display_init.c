#include "display_init.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"

#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PIXEL_CLOCK_HZ (14 * 1000 * 1000)
#define RGB_DATA_WIDTH 16
#define BOUNCE_BUFFER_HEIGHT 10
#define BOUNCE_BUFFER_SIZE (LCD_H_RES * BOUNCE_BUFFER_HEIGHT)

// Пины RGB
#define PIN_HSYNC 46
#define PIN_VSYNC 3
#define PIN_DE 5
#define PIN_PCLK 7
#define PIN_DATA0 14
#define PIN_DATA1 38
#define PIN_DATA2 18
#define PIN_DATA3 17
#define PIN_DATA4 10
#define PIN_DATA5 39
#define PIN_DATA6 0
#define PIN_DATA7 45
#define PIN_DATA8 48
#define PIN_DATA9 47
#define PIN_DATA10 21
#define PIN_DATA11 1
#define PIN_DATA12 2
#define PIN_DATA13 42
#define PIN_DATA14 41
#define PIN_DATA15 40

static const char *TAG = "display_init";

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

lv_display_t* display_init(void)
{
    ESP_LOGI(TAG, "Init RGB display 800x480");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.task_stack = 8192;
    port_cfg.task_affinity = 1;
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL240M,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 50,
            .vsync_back_porch = 70,
            .vsync_front_porch = 30,
            .flags = { .pclk_active_neg = 1 },
        },
        .data_width = RGB_DATA_WIDTH,
        .num_fbs = 1,
        .bounce_buffer_size_px = BOUNCE_BUFFER_SIZE,
        .hsync_gpio_num = PIN_HSYNC,
        .vsync_gpio_num = PIN_VSYNC,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3,
            PIN_DATA4, PIN_DATA5, PIN_DATA6, PIN_DATA7,
            PIN_DATA8, PIN_DATA9, PIN_DATA10, PIN_DATA11,
            PIN_DATA12, PIN_DATA13, PIN_DATA14, PIN_DATA15,
        },
        .flags = { 
            .fb_in_psram = 1,
            .double_fb = 0,
            .no_fb = 0,
        },
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Управление подсветкой УДАЛЕНО — теперь оно делается в main.c

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!disp) return NULL;
    lv_display_set_default(disp);

    size_t buf_size = LCD_H_RES * 40 * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return NULL;
    }
    
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_user_data(disp, panel_handle);

    ESP_LOGI(TAG, "Display ready");
    return disp;
}