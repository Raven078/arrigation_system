#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "wifi_sta.h"

static const char *TAG = "WIFI_STA";
static EventGroupHandle_t s_wifi_event_group;
static const int CONNECTED_BIT = BIT0;
static esp_ip4_addr_t s_sta_ip = {0};
static bool s_is_connected = false;

static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA запущен, подключаемся к внешней сети...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "Отключение от внешней сети, причина: %d", event->reason);
                s_is_connected = false;
                xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
                // Попытка переподключения через 5 секунд
                vTaskDelay(pdMS_TO_TICKS(5000));
                esp_wifi_connect();
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            s_sta_ip = event->ip_info.ip;
            s_is_connected = true;
            ESP_LOGI(TAG, "Получен IP-адрес от внешней сети: " IPSTR, IP2STR(&s_sta_ip));
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        }
    }
}

esp_err_t wifi_sta_init(void)
{
    // Проверяем, задан ли SSID внешней сети
    if (strlen(CONFIG_ESP_WIFI_STA_SSID) == 0) {
        ESP_LOGW(TAG, "SSID внешней сети не задан, пропускаем инициализацию STA");
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();

    // Регистрируем обработчик событий для STA
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        NULL));

    // Настройка STA
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_STA_SSID,
            .password = CONFIG_ESP_WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    if (strlen(CONFIG_ESP_WIFI_STA_PASSWORD) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));  // важный момент: включаем оба режима
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Ожидаем подключения с таймаутом
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(CONFIG_ESP_WIFI_STA_CONNECTION_TIMEOUT_MS));
    if (bits & CONNECTED_BIT) {
        ESP_LOGI(TAG, "Подключено к внешней сети '%s'", CONFIG_ESP_WIFI_STA_SSID);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Не удалось подключиться к внешней сети за %d мс",
                 CONFIG_ESP_WIFI_STA_CONNECTION_TIMEOUT_MS);
        return ESP_FAIL;
    }
}

bool wifi_sta_is_connected(void)
{
    return s_is_connected;
}

esp_ip4_addr_t wifi_sta_get_ip(void)
{
    return s_sta_ip;
}