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
#include "esp_lvgl_port.h"

extern const lv_image_dsc_t greenhouse;

static const char *TAG = "UI_POMODORO";
static lv_obj_t *main_scr = NULL;
static lv_obj_t *pomodoro_scr = NULL;
static lv_timer_t *auto_close_timer = NULL;
static lv_timer_t *client_check_timer = NULL;
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

// Элементы управления скоростью
static lv_obj_t *speed_label = NULL;
static lv_obj_t *speed_plus_btn = NULL;
static lv_obj_t *speed_minus_btn = NULL;
static int current_speed = 30;

// Второй график (насос и клапан)
static lv_obj_t *pump_valve_chart = NULL;
static lv_chart_series_t *s_pump_series = NULL;
static lv_chart_series_t *s_valve_series = NULL;

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
static char last_time_str[6] = "--:--";
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

static void speed_minus_btn_event_cb(lv_event_t *e) {
    if (current_speed > 0) {
        current_speed -= 5;
        if (current_speed < 0) current_speed = 0;
        lv_label_set_text_fmt(speed_label, "%d%%", current_speed);
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "pump_speed:%d", current_speed);
        ui_pomodoro_send_command("pomodoro", cmd);
        ESP_LOGI(TAG, "Speed changed to %d%%", current_speed);
    }
}

static void speed_plus_btn_event_cb(lv_event_t *e) {
    if (current_speed < 100) {
        current_speed += 5;
        if (current_speed > 100) current_speed = 100;
        lv_label_set_text_fmt(speed_label, "%d%%", current_speed);
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "pump_speed:%d", current_speed);
        ui_pomodoro_send_command("pomodoro", cmd);
        ESP_LOGI(TAG, "Speed changed to %d%%", current_speed);
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
    // Блокируем LVGL для безопасного обновления из задачи-обработчика очереди
    lvgl_port_lock(0);
    file_logger_update_chart_data();
    load_chart_data_from_global();
    refresh_chart();
    ui_pomodoro_update_pump_valve_chart();
    lvgl_port_unlock();
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
        strncpy(last_time_str, time_str, 5);
        last_time_str[5] = '\0';
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

void ui_pomodoro_update_speed(int speed) {
    if (speed < 0) speed = 0;
    if (speed > 100) speed = 100;
    current_speed = speed;
    if (speed_label && window_open) {
        lv_label_set_text_fmt(speed_label, "%d%%", speed);
    }
    ESP_LOGI(TAG, "Speed updated from JSON: %d%%", speed);
}

void ui_pomodoro_reset_client_check_timer(void) {
    if (client_check_timer) {
        lv_timer_reset(client_check_timer);
        ESP_LOGD(TAG, "Client check timer reset");
    }
}

// Обновление второго графика (с блокировкой)
void ui_pomodoro_update_pump_valve_chart(void) {
    if (!pump_valve_chart) return;
    lvgl_port_lock(0);
    int32_t *pump_data, *valve_data;
    int count;
    file_logger_get_pump_valve_data(&pump_data, &valve_data, &count);
    lv_chart_set_ext_y_array(pump_valve_chart, s_pump_series, pump_data);
    lv_chart_set_ext_y_array(pump_valve_chart, s_valve_series, valve_data);
    lv_chart_refresh(pump_valve_chart);
    lvgl_port_unlock();
}

static void close_pomodoro_window(void) {
    if (!pomodoro_scr) return;
    if (auto_close_timer) {
        lv_timer_del(auto_close_timer);
        auto_close_timer = NULL;
    }
    if (client_check_timer) {
        lv_timer_del(client_check_timer);
        client_check_timer = NULL;
    }
    ui_pomodoro_send_command("pomodoro", "stop_data");
    lv_obj_del(pomodoro_scr);
    pomodoro_scr = NULL;
    window_open = false;
    speed_label = NULL;
    speed_plus_btn = NULL;
    speed_minus_btn = NULL;
    pump_valve_chart = NULL;
    s_pump_series = NULL;
    s_valve_series = NULL;
    if (main_scr) lv_scr_load(main_scr);
    ESP_LOGI(TAG, "Pomodoro window closed");
}

static void close_btn_event_cb(lv_event_t *e) {
    close_pomodoro_window();
}

static void auto_close_timer_cb(lv_timer_t *timer) {
    close_pomodoro_window();
}

static void client_check_timer_cb(lv_timer_t *timer) {
    if (!window_open || !pomodoro_scr) return;
    if (!tcp_is_client_connected("pomodoro")) {
        ESP_LOGW(TAG, "Client 'pomodoro' lost, closing window");
        close_pomodoro_window();
    }
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

    // Фоновое изображение
    lv_obj_t *img = lv_image_create(pomodoro_scr);
    lv_image_set_src(img, &greenhouse);
    lv_obj_set_size(img, 800, 480);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_background(img);

    // Заголовок
    lv_obj_t *title = lv_label_create(pomodoro_scr);
    lv_label_set_text(title, "Теплица помидорник");
    lv_obj_set_style_text_font(title, &my_arial24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(title, LV_OPA_TRANSP, 0);
    lv_obj_set_width(title, 400);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Показатели температуры, влажности, времени
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
    lv_obj_set_pos(temp_unit_label, 190, 60);

    humi_value_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(humi_value_label, "--");
    lv_obj_set_style_text_font(humi_value_label, &my_arial24, 0);
    lv_obj_set_style_text_color(humi_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(humi_value_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(humi_value_label, 160, 115);

    humi_unit_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(humi_unit_label, "%");
    lv_obj_set_style_text_font(humi_unit_label, &my_arial24, 0);
    lv_obj_set_style_text_color(humi_unit_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(humi_unit_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(humi_unit_label, 200, 115);

    time_label = lv_label_create(pomodoro_scr);
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_font(time_label, &my_arial24, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(time_label, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(time_label, 160, 170);

    // Индикаторы
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

    // Кнопка закрытия
    lv_obj_t *close_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(close_btn, 30, 30);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_set_style_text_font(close_label, &my_arial24, 0);
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Кнопки управления насосом и клапаном
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

    // Управление скоростью
    speed_label = lv_label_create(pomodoro_scr);
    lv_label_set_text_fmt(speed_label, "%d%%", current_speed);
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(speed_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speed_label, 1, 0);
    lv_obj_set_style_border_color(speed_label, lv_color_black(), 0);
    lv_obj_set_style_pad_left(speed_label, 4, 0);
    lv_obj_set_style_pad_right(speed_label, 4, 0);
    lv_obj_set_width(speed_label, 50);
    lv_obj_align_to(speed_label, pump_ind, LV_ALIGN_OUT_TOP_MID, -30, -10);

    speed_minus_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(speed_minus_btn, 30, 30);
    lv_obj_align_to(speed_minus_btn, speed_label, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_t *minus_label = lv_label_create(speed_minus_btn);
    lv_label_set_text(minus_label, "-");
    lv_obj_set_style_text_font(minus_label, &my_arial24, 0);
    lv_obj_center(minus_label);
    lv_obj_add_event_cb(speed_minus_btn, speed_minus_btn_event_cb, LV_EVENT_CLICKED, NULL);

    speed_plus_btn = lv_btn_create(pomodoro_scr);
    lv_obj_set_size(speed_plus_btn, 30, 30);
    lv_obj_align_to(speed_plus_btn, speed_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_t *plus_label = lv_label_create(speed_plus_btn);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &my_arial24, 0);
    lv_obj_center(plus_label);
    lv_obj_add_event_cb(speed_plus_btn, speed_plus_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Контейнер для графиков
    lv_obj_t *chart_container = lv_obj_create(pomodoro_scr);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_pos(chart_container, 70, 227);
    lv_obj_set_size(chart_container, 720, 213);
    lv_obj_set_style_bg_opa(chart_container, LV_OPA_TRANSP, 0);

    // Основной график (температура и влажность)
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
    temp_series = lv_chart_add_series(chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_SECONDARY_Y);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);

    // Второй график (насос и клапан)
    pump_valve_chart = lv_chart_create(chart_container);
    lv_obj_set_size(pump_valve_chart, 720, 213);
    lv_obj_set_pos(pump_valve_chart, 0, 0);
    lv_chart_set_type(pump_valve_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(pump_valve_chart, CHART_POINTS);
    lv_chart_set_div_line_count(pump_valve_chart, 0, 0);
    lv_obj_set_style_bg_opa(pump_valve_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pump_valve_chart, 0, 0);
    lv_obj_set_style_pad_all(pump_valve_chart, 0, 0);
    lv_chart_set_range(pump_valve_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_size(pump_valve_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_remove_flag(pump_valve_chart, LV_OBJ_FLAG_CLICKABLE);

    s_pump_series = lv_chart_add_series(pump_valve_chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
    s_valve_series = lv_chart_add_series(pump_valve_chart, lv_color_hex(0xFFA500), LV_CHART_AXIS_PRIMARY_Y);

    // Загрузка данных в графики с блокировкой LVGL
    lvgl_port_lock(0);
    load_chart_data_from_global();
    refresh_chart();
    ui_pomodoro_update_pump_valve_chart();
    lvgl_port_unlock();

    // Обновление индикаторов, если есть данные
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

    // Запрос данных и запуск таймеров
    ESP_LOGI(TAG, "Requesting data from client pomodoro");
    tcp_send_command("pomodoro", "send_data");

    auto_close_timer = lv_timer_create(auto_close_timer_cb, 600000, NULL);
    lv_timer_set_repeat_count(auto_close_timer, 1);

    client_check_timer = lv_timer_create(client_check_timer_cb, 78000, NULL);
    lv_timer_set_repeat_count(client_check_timer, -1);

    ESP_LOGI(TAG, "Pomodoro window created, auto-close in 10 min, client check every 78s");
}