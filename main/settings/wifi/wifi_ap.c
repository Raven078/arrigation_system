#include "wifi_ap.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"          // для MAC2STR, MACSTR
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include <string.h>

static const char *TAG = "WIFI_AP";

#define AP_SSID     "ESP32_S3_Display"
#define AP_PASSWORD "12345678"
#define AP_MAX_CONN 4

void wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi SoftAP");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = 6,
        },
    };
    if (strlen(AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started. SSID: %s, IP: 192.168.4.1", AP_SSID);
}

void wifi_ap_get_station_list(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) return;
    buffer[0] = '\0';

    wifi_sta_list_t sta_list;
    memset(&sta_list, 0, sizeof(sta_list));
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    if (err != ESP_OK) {
        snprintf(buffer, buffer_size, "Ошибка получения списка");
        return;
    }

    if (sta_list.num == 0) {
        snprintf(buffer, buffer_size, "Нет подключенных устройств");
        return;
    }

    snprintf(buffer, buffer_size, "MAC адрес            IP адрес\n");
    for (int i = 0; i < sta_list.num; i++) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_list.sta[i].mac));
        int ip_last = i + 2;
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "192.168.4.%d", ip_last);
        char line[64];
        snprintf(line, sizeof(line), "%s  %s\n", mac_str, ip_str);
        strlcat(buffer, line, buffer_size);
    }
}