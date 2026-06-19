#include "ui_pomodoro.h"
#include "../fonts/my_arial24.h"
#include "../../file_logger.h"
#include "settings/wifi/tcp_server.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

extern const lv_image_dsc_t greenhouse;

static const char *TAG = "UI_POMODORO";
static lv_obj_t *main_scr = NULL;
static lv_obj_t *pomodoro_scr = NULL;
static lv_timer_t *auto_close_timer = NULL;
static lv_obj_t *temp_value_label = NULL;
static lv_obj_t *humi_value_label = NULL;
static lv_obj_t *temp_unit_label = NULL;
static lv_obj_t *humi_unit_label = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *sensor1_ind = NULL;
static lv_obj_t *sensor2_ind = NULL;
static lv_obj_t *pump_ind = NULL;
static lv_obj_t *valve_ind = NULL;
static lv_obj_t *pump_btn = NULL;
static lv_obj_t *valve_btn = NULL;
static lv_obj_t *chart = NULL;
static lv_chart_series_t *temp_series = NULL;
static lv_chart_series_t *humi_series = NULL;

static QueueHandle_t chart_reload_queue = NULL;

#define CHART_POINTS 144
static int32_t temp_values[CHART_POINTS] = {0};
static int32_t humi_values[CHART_POINTS] = {0};

static float last_temperature = 0.0f;
static int last_moisture = 0;
static bool last_sensor1 = false;
static bool last_sensor2 = false;
static bool last_valve_state = false;
static bool last_pump_state = false;
static char last_time_str[9] = "--:--";
static bool has_received_data = false;
static bool window_open = false;

static void close_pomodoro_window(void);
static void update_button_state(void);
static void refresh_chart(void);
static void load_chart_data_from_global(void);
static void process_chart_reload(void);

extern void tcp_send_command(const char *client_name, const char *cmd);

void ui_pomodoro_send_command(const char *client_name, const char *cmd) {
    tcp_send_command(client_name, cmd);
}

static void pump_btn_event_cb(lv_event_t *e) {
    if (last_pump_state) {
        ui_pomodoro_send_command("pomodoro", "pump_off");
        ESP_LOGI(TAG, "Command sent: pump_off to pomodoro");
    } else {
        ui_pomodoro_send_command("pomodoro", "pump_on");
        ESP_LOGI(TAG, "Command sent: pump_on to pomodoro");
    }
}

static void valve_btn_event_cb(lv_event_t *e) {
    if (last_valve_state) {
        ui_pomodoro_send_command("pomodoro", "valve_close");
        ESP_LOGI(TAG, "Command sent: valve_close to pomodoro");
    } else {
        ui_pomodoro_send_command("pomodoro", "valve_open");
        ESP_LOGI(TAG, "Command sent: valve_open to pomodoro");
    }
}

static void update_button_state(void) {
    if (!pump_btn || !valve_btn) return;
    if (last_pump_state) {
        lv_obj_set_style_bg_color(pump_btn, lv_color_hex(0xFF0000), 0);
        lv_obj_t *label = lv_obj_get_child(pump_btn, 0);
        if (label) lv_label_set_text(label, "OFF");
    } else {
        lv_obj_set_style_bg_color(pump_btn, lv_color_hex(0x00FF00), 0);
        lv_obj_t *label = lv_obj_get_child(pump_btn, 0);
        if (label) lv_label_set_text(label, "ON");
    }
    if (last_valve_state) {
        lv_obj_set_style_bg_color(valve_btn, lv_color_hex(0xFF0000), 0);
        lv_obj_t *label = lv_obj_get_child(valve_btn, 0);
        if (label) lv_label_set_text(label, "OFF");
    } else {
        lv_obj_set_style_bg_color(valve_btn, lv_color_hex(0x00FF00), 0);
        lv_obj_t *label = lv_obj_get_child(valve_btn, 0);
        if (label) lv_label_set_text(label, "ON");
    }
}

static void update_numeric_labels(void) {
    if (temp_value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", last_temperature);
        lv_label_set_text(temp_value_label, buf);
    }
    if (humi_value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", last_moisture);
        lv_label_set_text(humi_value_label, buf);
    }
    if (time_label) {
        lv_label_set_text(time_label, last_time_str);
    }
}

static void set_indicator_color(lv_obj_t *ind, bool detected, bool active_logic) {
    lv_color_t color;
    if (active_logic) {
        color = detected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    } else {
        color = detected ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00);
    }
    lv_obj_set_style_bg_color(ind, color, 0);
    lv_obj_set_style_border_width(ind, 0, 0);
}

static void set_indicator_idle(lv_obj_t *ind) {
    lv_obj_set_style_bg_color(ind, lv_color_white(), 0);
    lv_obj_set_style_border_width(ind, 1, 0);
    lv_obj_set_style_border_color(ind, lv_color_black(), 0);
}

static void update_all_indicators(void) {
    if (!pomodoro_scr) return;
    if (has_received_data) {
        set_indicator_color(sensor1_ind, last_sensor1, true);
        set_indicator_color(sensor2_ind, last_sensor2, true);
        set_indicator_color(pump_ind, last_pump_state, false);
        set_indicator_color(valve_ind, last_valve_state, false);
    } else {
        set_indicator_idle(sensor1_ind);
        set_indicator_idle(sensor2_ind);
        set_indicator_idle(pump_ind);
        set_indicator_idle(valve_ind);
    }
    update_button_state();
}

static void refresh_chart(void) {
    if (!chart) return;
    lv_chart_set_ext_y_array(chart, temp_series, temp_values);
    lv_chart_set_ext_y_array(chart, humi_series, humi_values);
    lv_chart_refresh(chart);
    ESP_LOGI(TAG, "Chart refreshed");
}

static void load_chart_data_from_global(void) {
    int32_t *global_temp, *global_humi;
    int point_count;
    file_logger_get_chart_data(&global_temp, &global_humi, &point_count);
    for (int i = 0; i < CHART_POINTS; i++) {
        temp_values[i] = global_temp[i];
        humi_values[i] = global_humi[i];
    }
    ESP_LOGI(TAG, "Chart data loaded from global storage, points=%d", point_count);
}

static void process_chart_reload(void) {
    file_logger_update_chart_data();
    load_chart_data_from_global();
    refresh_chart();
}

void ui_pomodoro_update_chart_from_logger(void) {
    if (!chart_reload_queue) return;
    int cmd = 1;
    if (xQueueSend(chart_reload_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Chart reload queue full");
    }
}

void ui_pomodoro_update_current(float temperature, int moisture,
                                bool sensor1_detected, bool sensor2_detected,
                                bool valve_state, bool pump_state,
                                const char *time_str) {
    last_temperature = temperature;
    last_moisture = moisture;
    last_sensor1 = sensor1_detected;
    last_sensor2 = sensor2_detected;
    last_valve_state = valve_state;
    last_pump_state = pump_state;
    if (time_str && strlen(time_str) >= 5) {
        strncpy(last_time_str, time_str, sizeof(last_time_str)-1);
        last_time_str[sizeof(last_time_str)-1] = '\0';
    } else {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(last_time_str, sizeof(last_time_str), "%H:%M", tm);
    }
    has_received_data = true;
    if (window_open && pomodoro_scr) {
        update_numeric_labels();
        update_all_indicators();
    }
}

static void close_pomodoro_window(void) {
    if (!pomodoro_scr) return;
    if (auto_close_timer) {
        lv_timer_del(auto_close_timer);
        auto_close_timer = NULL;
    }
    ui_pomodoro_send_command("pomodoro", "stop_data");
    lv_obj_del(pomodoro_scr);
    pomodoro_scr = NULL;
    window_open = false;
    if (main_scr) lv_scr_load(main_scr);
    ESP_LOGI(TAG, "Pomodoro window closed");
}

static void close_btn_event_cb(lv_event_t *e) {
    close_pomodoro_window();
}

static void auto_close_timer_cb(lv_timer_t *timer) {
    close_pomodoro_window();
}

static void chart_reload_task(void *pvParameters) {
    int cmd;
    while (1) {
        if (xQueueReceive(chart_reload_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            process_chart_reload();
        }
    }
}

void ui_pomodoro_show_window(void) {
    main_scr = lv_scr_act();
    pomodoro_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pomodoro_scr, lv_color_hex(0xE0F0FF), 0);
    lv_obj_set_style_bg_opa(pomodoro_scr, LV_OPA_COVER, 0);
    lv_scr_load(pomodoro_scr);
    window_open = true;

    if (!chart_reload_queue) {
        chart_reload_queue = xQueueCreate(5, sizeof(int));
        if (chart_reload_queue) {
            xTaskCreatePinnedToCore(chart_reload_task, "chart_reload_task", 4096, NULL, 3, NULL, 1);
        }
    }

    lv_obj_t *img = lv_image_create(pomodoro_scr);
    lv_image_set_src(img, &greenhouse);
    lv_obj_set_size(img, 800, 480);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_background(img);

    lv_obj_t *title = lv_label_create(pomodoro_scr);
    lv_label_set_text(title, "Теплица помидорник");
    lv_obj_set_style_text_font(title, &my_arial24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(title, LV_OPA_TRANSP, 0);
    lv_obj_set_width(title, 400);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    temp_value_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(temp_value_label, "--");
    lv_obj_set_style_text_font(temp_value_label, &my_arial24, 0);
    lv_obj_set_style_text_color(temp_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(temp_value_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(temp_value_label, 160, 60);

    temp_unit_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(temp_unit_label, "°C");
    lv_obj_set_style_text_font(temp_unit_label, &my_arial24, 0);
    lv_obj_set_style_text_color(temp_unit_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(temp_unit_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(temp_unit_label, 200, 60);

    humi_value_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(humi_value_label, "--");
    lv_obj_set_style_text_font(humi_value_label, &my_arial24, 0);
    lv_obj_set_style_text_color(humi_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(humi_value_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(humi_value_label, 160, 120);

    humi_unit_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(humi_unit_label, "%");
    lv_obj_set_style_text_font(humi_unit_label, &my_arial24, 0);
    lv_obj_set_style_text_color(humi_unit_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(humi_unit_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(humi_unit_label, 200, 120);

    time_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_font(time_label, &my_arial24, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(time_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(time_label, 160, 180);

    sensor1_ind = lv_obj_create(pomodoro_scr);
    lv_obj_set_size(sensor1_ind, 20, 20);
    lv_obj_set_style_radius(sensor1_ind, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(sensor1_ind, LV_OPA_COVER, 0);
    lv_obj_set_pos(sensor1_ind, 660, 40);

    sensor2_ind = lv_obj_create(pomodoro_scr);
    lv_obj_set_size(sensor2_ind, 20, 20);
    lv_obj_set_style_radius(sensor2_ind, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(sensor2_ind, LV_OPA_COVER, 0);
    lv_obj_set_pos(sensor2_ind, 660, 160);

    pump_ind = lv_obj_create(pomodoro_scr);
    lv_obj_set_size(pump_ind, 20, 20);
    lv_obj_set_style_radius(pump_ind, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pump_ind, LV_OPA_COVER, 0);
    lv_obj_set_pos(pump_ind, 580, 140);

    valve_ind = lv_obj_create(pomodoro_scr);
    lv_obj_set_size(valve_ind, 20, 20);
    lv_obj_set_style_radius(valve_ind, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(valve_ind, LV_OPA_COVER, 0);
    lv_obj_set_pos(valve_ind, 580, 20);

    lv_obj_t *close_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(close_btn, 30, 30);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_set_style_text_font(close_label, &my_arial24, 0);
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, NULL);

    pump_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(pump_btn, 55, 25);
    lv_obj_set_pos(pump_btn, 508, 162);
    lv_obj_t *pump_btn_label = lv_label_create(pump_btn);
    lv_label_set_text(pump_btn_label, "ON");
    lv_obj_set_style_text_font(pump_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(pump_btn_label);
    lv_obj_add_event_cb(pump_btn, pump_btn_event_cb, LV_EVENT_CLICKED, NULL);

    valve_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(valve_btn, 55, 25);
    lv_obj_set_pos(valve_btn, 508, 37);
    lv_obj_t *valve_btn_label = lv_label_create(valve_btn);
    lv_label_set_text(valve_btn_label, "ON");
    lv_obj_set_style_text_font(valve_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(valve_btn_label);
    lv_obj_add_event_cb(valve_btn, valve_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *chart_container = lv_obj_create(pomodoro_scr);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_pos(chart_container, 70, 227);
    lv_obj_set_size(chart_container, 720, 213);
    lv_obj_set_style_bg_opa(chart_container, LV_OPA_TRANSP, 0);

    chart = lv_chart_create(chart_container);
    lv_obj_set_size(chart, 720, 213);
    lv_obj_set_pos(chart, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 500);

    humi_series = lv_chart_add_series(chart, lv_color_hex(0x0000FF), LV_CHART_AXIS_PRIMARY_Y);
    temp_series = lv_chart_add_series(chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_SECONDARY_Y);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);

    load_chart_data_from_global();
    refresh_chart();

    if (has_received_data) {
        update_numeric_labels();
        update_all_indicators();
    } else {
        set_indicator_idle(sensor1_ind);
        set_indicator_idle(sensor2_ind);
        set_indicator_idle(pump_ind);
        set_indicator_idle(valve_ind);
        update_button_state();
    }

    // ---- Запрос данных у клиента ----
    ESP_LOGI(TAG, "Requesting data from client pomodoro");
    tcp_send_command("pomodoro", "send_data");

    auto_close_timer = lv_timer_create(auto_close_timer_cb, 600000, NULL);
    lv_timer_set_repeat_count(auto_close_timer, 1);

    ESP_LOGI(TAG, "Pomodoro window created, auto-close in 10 minutes");
}