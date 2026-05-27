#ifndef UI_POMODORO_H
#define UI_POMODORO_H

void ui_pomodoro_show_window(void);
void ui_pomodoro_update_sensors(float temperature, int moisture, bool sensor1_detected, bool sensor2_detected);

#endif