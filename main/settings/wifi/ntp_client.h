#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

/**
 * @brief Инициализация NTP-клиента и синхронизация времени
 * 
 * Функция запускает задачу, которая один раз получает время с NTP-сервера
 * и устанавливает системное время. При необходимости можно вызывать повторно.
 * 
 * @return ESP_OK если успешно, иначе код ошибки.
 */
esp_err_t ntp_sync_init(void);

/**
 * @brief Получить текущее локальное время в виде строки
 * 
 * @param buffer Буфер для строки (должен быть не менее 64 байт)
 * @param max_len Размер буфера
 */
void ntp_get_time_str(char *buffer, size_t max_len);

/**
 * @brief Получить текущее время как time_t (UNIX timestamp)
 */
time_t ntp_get_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* NTP_CLIENT_H */