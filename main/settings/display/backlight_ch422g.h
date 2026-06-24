#ifndef BACKLIGHT_CH422G_H
#define BACKLIGHT_CH422G_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t backlight_ch422g_init(void);
esp_err_t backlight_ch422g_set(bool on);
void backlight_ch422g_touch_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_CH422G_H */