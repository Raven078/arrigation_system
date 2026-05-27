#ifndef UI_H
#define UI_H

#include "lvgl.h"

/**
 * @brief Создает фоновое изображение и кнопки.
 */
void ui_create_wallpaper(void);

/**
 * @brief Обрабатывает сенсорные события (заглушка, так как используется прямой callback от кнопок).
 */
void ui_handle_touch(void);

/**
 * @brief Возвращает указатель на главный экран LVGL.
 * 
 * @return lv_obj_t* указатель на объект главного экрана.
 */
lv_obj_t *ui_get_main_screen(void);

/**
 * @brief Callback для таймера (оставлен для совместимости, но не используется).
 * 
 * @param timer Указатель на таймер LVGL.
 */
void update_time_timer_cb(lv_timer_t *timer);

#endif /* UI_H */