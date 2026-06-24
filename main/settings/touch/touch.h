#ifndef TOUCH_H
#define TOUCH_H

#include "lvgl.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

extern i2c_master_bus_handle_t g_i2c_bus_handle;

void touch_init(lv_display_t *disp);
void touch_reinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_H */