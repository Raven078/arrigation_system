#include "ui.h"
#include "../wallpaper/wallpaper800400.h"   // относительный путь к wallpaper
#include "../fonts/my_arial24.h"
#include "../../settings/time/rtc_time.h"
#include "ui1.h"                            // тот же каталог, что и ui.c
#include "ui_pomodoro.h"
#include "ui_cucumber.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

static const char *TAG = "UI";
static lv_obj_t *main_screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_timer_t *update_timer = NULL;

static void ap_button_event_handler(lv_event_t *e);
static void tomato_button_event_handler(lv_event_t *e);
static void cucumber_button_event_handler(lv_event_t *e);

static void apply_button_style(lv_obj_t *btn) {
    lv_color_t bg_color = lv_color_hex(0x2ca034);
    lv_opa_t bg_opa = LV_OPA_80;
    lv_color_t border_color = lv_color_white();
    lv_coord_t border_width = 2;

    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_bg_opa(btn, bg_opa, 0);
    lv_obj_set_style_border_color(btn, border_color, 0);
    lv_obj_set_style_border_width(btn, border_width, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 3, LV_STATE_PRESSED);
}

static void update_datetime_cb(lv_timer_t *timer) {
    (void)timer;
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char time_str[6];
    char date_str[11];
    strftime(time_str, sizeof(time_str), "%H:%M", &tm_now);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", &tm_now);

    if (time_label) lv_label_set_text(time_label, time_str);
    if (date_label) lv_label_set_text(date_label, date_str);
}

void ui_create_wallpaper(void) {
    ESP_LOGI(TAG, "UI initialization started");
    vTaskDelay(pdMS_TO_TICKS(50));

    main_screen = lv_scr_act();
    lv_obj_clean(main_screen);

    if (wallpaper800400.data != NULL && wallpaper800400.data_size > 0) {
        lv_obj_t *wallpaper_img = lv_image_create(main_screen);
        lv_image_set_src(wallpaper_img, &wallpaper800400);
        lv_obj_set_pos(wallpaper_img, 0, 0);
        lv_obj_set_size(wallpaper_img, 800, 480);
        lv_obj_remove_flag(wallpaper_img, LV_OBJ_FLAG_CLICKABLE);
        ESP_LOGI(TAG, "Background image loaded.");
    } else {
        lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000033), 0);
        lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);
        ESP_LOGW(TAG, "Background image missing, using solid color.");
    }

    // Кнопка "Точка доступа"
    lv_obj_t *ap_btn = lv_btn_create(main_screen);
    lv_obj_set_size(ap_btn, 220, 70);
    lv_obj_set_pos(ap_btn, 30, 200);
    lv_obj_t *ap_label = lv_label_create(ap_btn);
    lv_label_set_text(ap_label, "Точка доступа");
    lv_obj_set_style_text_font(ap_label, &my_arial24, 0);
    lv_obj_center(ap_label);
    lv_obj_add_event_cb(ap_btn, ap_button_event_handler, LV_EVENT_CLICKED, NULL);
    apply_button_style(ap_btn);

    // Кнопка "Помидорник"
    lv_obj_t *tomato_btn = lv_btn_create(main_screen);
    lv_obj_set_size(tomato_btn, 220, 70);
    lv_obj_set_pos(tomato_btn, 290, 200);
    lv_obj_t *tomato_label = lv_label_create(tomato_btn);
    lv_label_set_text(tomato_label, "Помидорник");
    lv_obj_set_style_text_font(tomato_label, &my_arial24, 0);
    lv_obj_center(tomato_label);
    lv_obj_add_event_cb(tomato_btn, tomato_button_event_handler, LV_EVENT_CLICKED, NULL);
    apply_button_style(tomato_btn);

    // Кнопка "Огуречник"
    lv_obj_t *cucumber_btn = lv_btn_create(main_screen);
    lv_obj_set_size(cucumber_btn, 220, 70);
    lv_obj_set_pos(cucumber_btn, 550, 200);
    lv_obj_t *cucumber_label = lv_label_create(cucumber_btn);
    lv_label_set_text(cucumber_label, "Огуречник");
    lv_obj_set_style_text_font(cucumber_label, &my_arial24, 0);
    lv_obj_center(cucumber_label);
    lv_obj_add_event_cb(cucumber_btn, cucumber_button_event_handler, LV_EVENT_CLICKED, NULL);
    apply_button_style(cucumber_btn);

    // Стиль для времени/даты
    static lv_style_t style_text;
    lv_style_init(&style_text);
    lv_style_set_text_color(&style_text, lv_color_white());
    lv_style_set_text_font(&style_text, &lv_font_montserrat_14);
    lv_style_set_text_opa(&style_text, LV_OPA_COVER);
    // Тень для читаемости
    lv_style_set_shadow_width(&style_text, 2);
    lv_style_set_shadow_color(&style_text, lv_color_black());
    lv_style_set_shadow_opa(&style_text, LV_OPA_70);
    lv_style_set_shadow_offset_x(&style_text, 1);
    lv_style_set_shadow_offset_y(&style_text, 1);

    time_label = lv_label_create(main_screen);
    lv_label_set_text(time_label, "--:--");
    lv_obj_add_style(time_label, &style_text, 0);
    lv_obj_set_pos(time_label, 700, 440);

    date_label = lv_label_create(main_screen);
    lv_label_set_text(date_label, "--.--.----");
    lv_obj_add_style(date_label, &style_text, 0);
    lv_obj_set_pos(date_label, 690, 460);

    update_timer = lv_timer_create(update_datetime_cb, 60000, NULL);
    update_datetime_cb(NULL);

    ESP_LOGI(TAG, "UI with time/date created.");
}

static void ap_button_event_handler(lv_event_t *e) {
    ESP_LOGW(TAG, ">>> Button 'Access Point' pressed <<<");
    ui1_show_access_point_window();
}

static void tomato_button_event_handler(lv_event_t *e) {
    ESP_LOGW(TAG, ">>> Button 'Tomato' pressed <<<");
    ui_pomodoro_show_window();
}

static void cucumber_button_event_handler(lv_event_t *e) {
    ESP_LOGW(TAG, ">>> Button 'Cucumber' pressed <<<");
    ui_cucumber_show_window();
}

void ui_handle_touch(void) {}
lv_obj_t *ui_get_main_screen(void) {
    if (!main_screen) main_screen = lv_scr_act();
    return main_screen;
}
void update_time_timer_cb(lv_timer_t *timer) {
    (void)timer;
}