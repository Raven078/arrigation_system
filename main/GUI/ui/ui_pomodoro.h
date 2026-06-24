#ifndef UI_POMODORO_H
#define UI_POMODORO_H

#include "lvgl.h"

void ui_pomodoro_show_window(void);
void ui_pomodoro_update_current(float temperature, int moisture,
                                bool sensor1_detected, bool sensor2_detected,
                                bool valve_state, bool pump_state,
                                const char *time_str);
void ui_pomodoro_update_chart_from_logger(void);
void ui_pomodoro_update_speed(int speed);
void ui_pomodoro_update_pump_valve_chart(void);
void ui_pomodoro_reset_client_check_timer(void);

#endif