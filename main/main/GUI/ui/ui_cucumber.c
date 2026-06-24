#include "ui_cucumber.h"
#include "../fonts/my_arial24.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "UI_CUCUMBER";

static lv_obj_t *main_scr = NULL;
static lv_obj_t *cucumber_scr = NULL;

static lv_obj_t *chart = NULL;
static lv_chart_series_t *temp_series = NULL;
static lv_chart_series_t *humi_series = NULL;

static lv_obj_t *sensor1_indicator = NULL;
static lv_obj_t *sensor2_indicator = NULL;

#define CHART_POINTS 144   // 24 часа * 6 (каждые 10 минут)

static int32_t temp_values[CHART_POINTS] = {0};
static int32_t humi_values[CHART_POINTS] = {0};

// Статические массивы для осей и линий
static lv_point_precise_t sep_points[2] = {{5, 220}, {795, 220}};      // синяя разделительная линия
static lv_point_precise_t time_axis_points[2] = {{50, 460}, {770, 460}};
static lv_point_precise_t humi_axis_points[2] = {{50, 260}, {50, 460}};
static lv_point_precise_t temp_axis_points[2] = {{760, 260}, {760, 460}};

// --- Обновление данных графика ---
static void update_chart(float temp, int humi) {
    for (int i = 0; i < CHART_POINTS - 1; i++) {
        temp_values[i] = temp_values[i+1];
        humi_values[i] = humi_values[i+1];
    }
    temp_values[CHART_POINTS - 1] = (int32_t)(temp * 10);
    humi_values[CHART_POINTS - 1] = humi;

    lv_chart_set_ext_y_array(chart, temp_series, temp_values);
    lv_chart_set_ext_y_array(chart, humi_series, humi_values);
    lv_chart_refresh(chart);
}

// --- Индикаторы датчиков ---
static void update_indicators(bool s1, bool s2) {
    lv_color_t color1 = s1 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    lv_color_t color2 = s2 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    lv_obj_set_style_bg_color(sensor1_indicator, color1, 0);
    lv_obj_set_style_bg_color(sensor2_indicator, color2, 0);
}

// --- Публичная функция обновления ---
void ui_cucumber_update_sensors(float temperature, int moisture, bool sensor1_detected, bool sensor2_detected) {
    if (cucumber_scr == NULL) return;
    update_chart(temperature, moisture);
    update_indicators(sensor1_detected, sensor2_detected);
}

// --- Закрытие окна ---
static void close_btn_event_cb(lv_event_t *e) {
    if (cucumber_scr) {
        lv_obj_del(cucumber_scr);
        cucumber_scr = NULL;
    }
    if (main_scr) {
        lv_scr_load(main_scr);
    }
}

// --- Создание окна ---
void ui_cucumber_show_window(void) {
    main_scr = lv_scr_act();

    cucumber_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(cucumber_scr, lv_color_hex(0xE0F0FF), 0);
    lv_obj_set_style_bg_opa(cucumber_scr, LV_OPA_COVER, 0);
    lv_scr_load(cucumber_scr);

    // --- Прямоугольник с индикаторами ---
    static lv_style_t style_box;
    lv_style_init(&style_box);
    lv_style_set_border_width(&style_box, 1);
    lv_style_set_border_color(&style_box, lv_color_black());
    lv_style_set_radius(&style_box, 5);
    lv_style_set_bg_opa(&style_box, LV_OPA_50);
    lv_style_set_bg_color(&style_box, lv_color_white());

    lv_obj_t *right_box = lv_obj_create(cucumber_scr);
    lv_obj_add_style(right_box, &style_box, 0);
    lv_obj_set_size(right_box, 70, 150);
    lv_obj_align(right_box, LV_ALIGN_TOP_RIGHT, -60, 20);

    sensor1_indicator = lv_obj_create(right_box);
    lv_obj_set_size(sensor1_indicator, 10, 10);
    lv_obj_set_style_radius(sensor1_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(sensor1_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sensor1_indicator, 0, 0);
    lv_obj_align(sensor1_indicator, LV_ALIGN_TOP_LEFT, 5, 5);

    sensor2_indicator = lv_obj_create(right_box);
    lv_obj_set_size(sensor2_indicator, 10, 10);
    lv_obj_set_style_radius(sensor2_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(sensor2_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sensor2_indicator, 0, 0);
    lv_obj_align(sensor2_indicator, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    update_indicators(false, false);

    // --- Кнопка закрытия ---
    lv_obj_t *close_btn = lv_btn_create(cucumber_scr);
    lv_obj_set_size(close_btn, 40, 40);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_set_style_text_font(close_label, &my_arial24, 0);
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Разделительная линия (синяя, от 5,220 до 795,220) ---
    lv_obj_t *sep_line = lv_line_create(cucumber_scr);
    lv_line_set_points(sep_line, sep_points, 2);
    lv_obj_set_style_line_width(sep_line, 2, 0);
    lv_obj_set_style_line_color(sep_line, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_line_opa(sep_line, LV_OPA_COVER, 0);

    // --- Ось времени ---
    lv_obj_t *time_axis = lv_line_create(cucumber_scr);
    lv_line_set_points(time_axis, time_axis_points, 2);
    lv_obj_set_style_line_width(time_axis, 2, 0);
    lv_obj_set_style_line_color(time_axis, lv_color_black(), 0);

    int start_x = 50, end_x = 770;
    float step_x = (float)(end_x - start_x) / 24.0f;
    for (int hour = 0; hour <= 24; hour++) {
        int x = start_x + (int)(hour * step_x);
        lv_point_precise_t tick_pts[] = {{x, 460}, {x, 450}};
        lv_obj_t *tick = lv_line_create(cucumber_scr);
        lv_line_set_points(tick, tick_pts, 2);
        lv_obj_set_style_line_width(tick, 1, 0);
        lv_obj_set_style_line_color(tick, lv_color_black(), 0);
        if (hour < 24) {
            lv_obj_t *label = lv_label_create(cucumber_scr);
            char buf[4];
            snprintf(buf, sizeof(buf), "%02d", hour);
            lv_label_set_text(label, buf);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(label, lv_color_black(), 0);
            lv_obj_set_pos(label, x - 10, 462);
        }
    }
    for (int i = 0; i <= 24*6; i++) {
        if (i % 6 == 0) continue;
        int x = start_x + (int)((float)i / 6.0f * step_x);
        lv_point_precise_t tick_pts[] = {{x, 460}, {x, 455}};
        lv_obj_t *tick = lv_line_create(cucumber_scr);
        lv_line_set_points(tick, tick_pts, 2);
        lv_obj_set_style_line_width(tick, 1, 0);
        lv_obj_set_style_line_color(tick, lv_color_black(), 0);
    }

    // --- Ось влажности ---
    lv_obj_t *humi_axis = lv_line_create(cucumber_scr);
    lv_line_set_points(humi_axis, humi_axis_points, 2);
    lv_obj_set_style_line_width(humi_axis, 2, 0);
    lv_obj_set_style_line_color(humi_axis, lv_color_black(), 0);

    int humi_h = 200; // от 260 до 460
    for (int h = 0; h <= 100; h += 20) {
        int y = 460 - (h * humi_h / 100);
        lv_point_precise_t tick_pts[] = {{50, y}, {60, y}};
        lv_obj_t *tick = lv_line_create(cucumber_scr);
        lv_line_set_points(tick, tick_pts, 2);
        lv_obj_set_style_line_width(tick, 1, 0);
        lv_obj_set_style_line_color(tick, lv_color_black(), 0);
        lv_obj_t *label = lv_label_create(cucumber_scr);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", h);
        lv_label_set_text(label, buf);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(label, 18, y - 8);
    }
    lv_obj_t *humi_label = lv_label_create(cucumber_scr);
    lv_label_set_text(humi_label, "Влажность (%)");
    lv_obj_set_style_text_font(humi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(humi_label, lv_color_hex(0x0000FF), 0);
    lv_obj_set_pos(humi_label, 18, 240);

    // --- Ось температуры ---
    lv_obj_t *temp_axis = lv_line_create(cucumber_scr);
    lv_line_set_points(temp_axis, temp_axis_points, 2);
    lv_obj_set_style_line_width(temp_axis, 2, 0);
    lv_obj_set_style_line_color(temp_axis, lv_color_black(), 0);

    for (int t = 0; t <= 50; t += 10) {
        int y = 460 - (t * humi_h / 50);
        lv_point_precise_t tick_pts[] = {{760, y}, {750, y}};
        lv_obj_t *tick = lv_line_create(cucumber_scr);
        lv_line_set_points(tick, tick_pts, 2);
        lv_obj_set_style_line_width(tick, 1, 0);
        lv_obj_set_style_line_color(tick, lv_color_black(), 0);
        lv_obj_t *label = lv_label_create(cucumber_scr);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t);
        lv_label_set_text(label, buf);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(label, 770, y - 8);
    }
    lv_obj_t *temp_label = lv_label_create(cucumber_scr);
    lv_label_set_text(temp_label, "Температура (C)");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFF0000), 0);
    lv_obj_set_pos(temp_label, 670, 240);

    // --- Область графика ---
    lv_obj_t *chart_container = lv_obj_create(cucumber_scr);
    lv_obj_remove_style_all(chart_container);
    lv_obj_set_pos(chart_container, 50, 260);
    lv_obj_set_size(chart_container, 710, 200);
    lv_obj_set_style_bg_opa(chart_container, LV_OPA_TRANSP, 0);

    chart = lv_chart_create(chart_container);
    lv_obj_set_size(chart, 710, 200);
    lv_obj_set_pos(chart, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 500);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);

    // --- Убираем точки на графике (маркеры) ---
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(chart, 0, LV_PART_INDICATOR);

    // Серии данных
    humi_series = lv_chart_add_series(chart, lv_color_hex(0x0000FF), LV_CHART_AXIS_PRIMARY_Y);
    temp_series = lv_chart_add_series(chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_SECONDARY_Y);

    // Тестовые данные (синусоиды)
    for (int i = 0; i < CHART_POINTS; i++) {
        double angle = 2 * M_PI * i / CHART_POINTS;
        double temp_sin = 25 + 10 * sin(angle);
        temp_values[i] = (int32_t)(temp_sin * 10);
        humi_values[i] = 60 + (int)(20 * sin(angle * 0.8));
    }
    lv_chart_set_ext_y_array(chart, temp_series, temp_values);
    lv_chart_set_ext_y_array(chart, humi_series, humi_values);
    lv_chart_refresh(chart);

    ESP_LOGI(TAG, "Cucumber window created: points hidden, secondary axis range 0-500, blue separator line at y=220.");
}