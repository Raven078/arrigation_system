// main/settings/wifi/wifi_ap.h
#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stddef.h>

void wifi_init_softap(void);
void wifi_ap_get_station_list(char *buffer, size_t buffer_size);

#endif