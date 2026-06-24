#include "ui1.h"
#include "../../settings/wifi/wifi_ap.h"
#include "../fonts/my_arial24.h"   // добавлен для использования шрифта
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "UI1";
static lv_obj_t *main_scr = NULL;
static lv_obj_t *ap_scr = NULL;
static lv_obj_t *password_label = NULL;
static bool password_hidden = true;
static lv_obj_t *stations_label = NULL;

static void password_label_event_cb(lv_event_t *e) {
    if (password_hidden) {
        lv_label_set_text(password_label, "12345678");
        password_hidden = false;
    } else {
        lv_label_set_text(password_label, "********");
        password_hidden = true;
    }
}

static void update_stations_list(void) {
    char buffer[512] = {0};
    wifi_ap_get_station_list(buffer, sizeof(buffer));
    if (strlen(buffer) == 0) {
        lv_label_set_text(stations_label, "Нет подключенных устройств");
    } else {
        lv_label_set_text(stations_label, buffer);
    }
}

static void refresh_btn_event_cb(lv_event_t *e) {
    update_stations_list();
}

static void close_btn_event_cb(lv_event_t *e) {
    if (ap_scr) {
        lv_obj_del(ap_scr);
        ap_scr = NULL;
    }
    if (main_scr) {
        lv_scr_load(main_scr);
    }
}

void ui1_show_access_point_window(void) {
    main_scr = lv_scr_act();

    ap_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ap_scr, lv_color_hex(0xE0F0FF), 0);
    lv_obj_set_style_bg_opa(ap_scr, LV_OPA_COVER, 0);
    lv_scr_load(ap_scr);

    // Заголовок – шрифт my_arial24
    lv_obj_t *title = lv_label_create(ap_scr);
    lv_label_set_text(title, "Информация о точке доступа");
    lv_obj_set_style_text_font(title, &my_arial24, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // SSID
    lv_obj_t *ssid_label = lv_label_create(ap_scr);
    lv_label_set_text(ssid_label, "Wi-Fi: ESP32_S3_Display");
    lv_obj_set_style_text_font(ssid_label, &my_arial24, 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 80);

    // IP
    lv_obj_t *ip_label = lv_label_create(ap_scr);
    lv_label_set_text(ip_label, "IP: 192.168.4.1");
    lv_obj_set_style_text_font(ip_label, &my_arial24, 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 20, 110);

    // Порт
    lv_obj_t *port_label = lv_label_create(ap_scr);
    lv_label_set_text(port_label, "Порт: 8888");
    lv_obj_set_style_text_font(port_label, &my_arial24, 0);
    lv_obj_align(port_label, LV_ALIGN_TOP_LEFT, 20, 140);

    // Заголовок "Пароль:"
    lv_obj_t *password_title = lv_label_create(ap_scr);
    lv_label_set_text(password_title, "Пароль:");
    lv_obj_set_style_text_font(password_title, &my_arial24, 0);
    lv_obj_align(password_title, LV_ALIGN_TOP_LEFT, 20, 170);

    // Метка пароля (кликабельная)
    password_label = lv_label_create(ap_scr);
    lv_label_set_text(password_label, "********");
    lv_obj_set_style_text_font(password_label, &my_arial24, 0);
    lv_obj_set_style_text_color(password_label, lv_color_hex(0x0000FF), 0);
    lv_obj_add_flag(password_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(password_label, password_label_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align_to(password_label, password_title, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Разделитель
    lv_obj_t *line = lv_obj_create(ap_scr);
    lv_obj_set_size(line, 700, 2);
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 20, 200);

    // Заголовок списка устройств
    lv_obj_t *stations_title = lv_label_create(ap_scr);
    lv_label_set_text(stations_title, "Подключенные устройства:");
    lv_obj_set_style_text_font(stations_title, &my_arial24, 0);
    lv_obj_align(stations_title, LV_ALIGN_TOP_LEFT, 20, 230);

    // Метка списка устройств
    stations_label = lv_label_create(ap_scr);
    lv_label_set_text(stations_label, "Обновление...");
    lv_obj_set_width(stations_label, 750);
    lv_obj_set_style_text_font(stations_label, &my_arial24, 0);
    lv_obj_align_to(stations_label, stations_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    // Кнопка "Обновить"
    lv_obj_t *refresh_btn = lv_btn_create(ap_scr);
    lv_obj_set_size(refresh_btn, 100, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "Обновить");
    lv_obj_set_style_text_font(refresh_label, &my_arial24, 0);
    lv_obj_center(refresh_label);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Кнопка "Закрыть"
    lv_obj_t *close_btn = lv_btn_create(ap_scr);
    lv_obj_set_size(close_btn, 100, 40);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Закрыть");
    lv_obj_set_style_text_font(close_label, &my_arial24, 0);
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, NULL);

    update_stations_list();
    ESP_LOGI(TAG, "Access point window opened");
}