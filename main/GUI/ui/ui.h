#ifndef UI_H
#define UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_create_wallpaper(void);
void ui_handle_touch(void);
lv_obj_t *ui_get_main_screen(void);
void update_time_timer_cb(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */