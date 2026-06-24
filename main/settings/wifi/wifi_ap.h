#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stddef.h>
#include <stdbool.h>   // <-- ДОБАВЛЕНО для bool

void wifi_init_softap(void);
void wifi_ap_get_station_list(char *buffer, size_t buffer_size);
bool wifi_ap_is_ip_assigned(const char *ip_str);   // <-- НОВАЯ ФУНКЦИЯ

#endif