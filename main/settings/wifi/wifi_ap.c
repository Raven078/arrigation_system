#include "wifi_ap.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "settings/time/rtc_time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WIFI_AP";

#define AP_SSID "ESP32_S3_Display"
#define AP_PASSWORD "12345678"
#define AP_MAX_CONN 4

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "STA: connecting to AP...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "STA: disconnected, reason: %d", event->reason);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
        ESP_LOGI(TAG, "STA: retrying...");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "STA: got IP:" IPSTR, IP2STR(&event->ip_info.ip));
#ifdef CONFIG_WIFI_STA_ENABLE
        rtc_time_sync_from_ntp();
#endif
    }
}

void wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi SoftAP");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Создаём интерфейс для SoftAP
    esp_netif_create_default_wifi_ap();

#ifdef CONFIG_WIFI_STA_ENABLE
    // === СОЗДАЁМ ИНТЕРФЕЙС ДЛЯ STA ===
    esp_netif_create_default_wifi_sta();
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t ap_config = {
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
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

#ifdef CONFIG_WIFI_STA_ENABLE
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
#else
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
#endif

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

#ifdef CONFIG_WIFI_STA_ENABLE
    wifi_config_t sta_config = {
        .sta = {
            .ssid = CONFIG_WIFI_STA_SSID,
            .password = CONFIG_WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_LOGI(TAG, "STA config set to %s", CONFIG_WIFI_STA_SSID);
#endif

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

    snprintf(buffer, buffer_size, "MAC адрес\tIP адрес\n");
    for (int i = 0; i < sta_list.num; i++) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_list.sta[i].mac));

        int ip_last = i + 2;
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "192.168.4.%d", ip_last);

        char line[64];
        snprintf(line, sizeof(line), "%s\t%s\n", mac_str, ip_str);
        strlcat(buffer, line, buffer_size);
    }
}

bool wifi_ap_is_ip_assigned(const char *ip_str)
{
    if (!ip_str) return false;

    wifi_sta_list_t sta_list;
    memset(&sta_list, 0, sizeof(sta_list));
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get station list for IP check");
        return false;
    }

    for (int i = 0; i < sta_list.num; i++) {
        char ip_buf[32];
        snprintf(ip_buf, sizeof(ip_buf), "192.168.4.%d", i + 2);
        if (strcmp(ip_str, ip_buf) == 0) {
            return true;
        }
    }
    return false;
}