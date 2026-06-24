#ifndef WIFI_STA_H
#define WIFI_STA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_netif.h"

/**
 * @brief Инициализация Wi-Fi в режиме клиента (STA)
 * 
 * Функция подключается к внешней сети с параметрами из Kconfig.
 * После подключения IP-адрес будет получен по DHCP.
 * 
 * @return ESP_OK если успешно, иначе код ошибки.
 */
esp_err_t wifi_sta_init(void);

/**
 * @brief Проверить, подключено ли устройство к внешней сети
 * 
 * @return true если подключено, false иначе
 */
bool wifi_sta_is_connected(void);

/**
 * @brief Получить IP-адрес, выданный внешним роутером
 * 
 * @return esp_ip4_addr_t IP-адрес (0.0.0.0 если не подключено)
 */
esp_ip4_addr_t wifi_sta_get_ip(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_H */