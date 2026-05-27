#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "tcp_server.h"
#include "rtc_time.h"

static const char *TAG = "TCP_SERVER";
#define MAX_CLIENTS 5

static void handle_client_wrapper(void *pvParameters);

static void handle_client(int sock)
{
    char rx_buffer[256];
    int len;

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Ошибка приёма: errno %d", errno);
            break;
        } else if (len == 0) {
            ESP_LOGI(TAG, "Клиент закрыл соединение");
            break;
        } else {
            rx_buffer[len] = '\0';
            // Удаляем символ новой строки если есть
            if (rx_buffer[len-1] == '\n') {
                rx_buffer[len-1] = '\0';
                len--;
            }
            if (rx_buffer[len-1] == '\r') {
                rx_buffer[len-1] = '\0';
                len--;
            }
            
            ESP_LOGI(TAG, "Получена команда: %s (длина %d)", rx_buffer, len);
            
            // Обработка команды GET_TIME
            if (strcmp(rx_buffer, "GET_TIME") == 0) {
                char time_str[64];
                rtc_time_get_str(time_str, sizeof(time_str));
                char response[128];
                snprintf(response, sizeof(response), "TIME:%s\n", time_str);
                send(sock, response, strlen(response), 0);
                ESP_LOGI(TAG, "Отправлено время: %s", time_str);
            }
            else if (strcmp(rx_buffer, "GET_TIMESTAMP") == 0) {
                time_t timestamp = rtc_time_get_timestamp();
                char response[64];
                snprintf(response, sizeof(response), "TIMESTAMP:%lld\n", (long long)timestamp);
                send(sock, response, strlen(response), 0);
                ESP_LOGI(TAG, "Отправлен timestamp: %lld", (long long)timestamp);
            }
            else {
                const char *response = "Неизвестная команда. Доступные команды: GET_TIME, GET_TIMESTAMP\n";
                send(sock, response, strlen(response), 0);
            }
        }
    } while (len > 0);

    shutdown(sock, 0);
    close(sock);
}

static void handle_client_wrapper(void *pvParameters)
{
    int sock = (intptr_t)pvParameters;
    handle_client(sock);
    vTaskDelete(NULL);
}

static void tcp_server_task(void *pvParameters)
{
    uint16_t port = *((uint16_t*)pvParameters);
    free(pvParameters);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Не удалось создать сокет: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Ошибка bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, MAX_CLIENTS) != 0) {
        ESP_LOGE(TAG, "Ошибка listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP-сервер запущен на порту %d", port);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Ошибка accept: errno %d", errno);
            continue;
        }

        char addr_str[16];
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Новое подключение от %s:%d", addr_str, source_addr.sin_port);

        xTaskCreate(handle_client_wrapper, "tcp_client", 4096, (void*)(intptr_t)client_sock, 5, NULL);
    }
}

void tcp_server_start(uint16_t port)
{
    uint16_t *port_ptr = malloc(sizeof(uint16_t));
    *port_ptr = port;
    xTaskCreate(tcp_server_task, "tcp_server", 4096, port_ptr, 5, NULL);
}