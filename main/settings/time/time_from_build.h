#ifndef TIME_FROM_BUILD_H
#define TIME_FROM_BUILD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

/**
 * @brief Установить системное время из времени компиляции прошивки
 * 
 * Функция парсит макросы __DATE__ и __TIME__ и устанавливает системное время.
 * Точность: до секунды, момент компиляции.
 * 
 * @return ESP_OK если успешно, иначе ESP_FAIL
 */
esp_err_t time_init_from_build(void);

/**
 * @brief Получить текущее системное время в виде строки
 * 
 * @param buffer Буфер для строки (минимум 64 байта)
 * @param max_len Размер буфера
 */
void time_get_str(char *buffer, size_t max_len);

/**
 * @brief Получить текущее время как time_t (UNIX timestamp)
 */
time_t time_get_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* TIME_FROM_BUILD_H */