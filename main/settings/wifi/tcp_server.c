#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "tcp_server.h"
#include "rtc_time.h"
#include "GUI/ui/ui_pomodoro.h"
#include "GUI/ui/ui_cucumber.h"
#include "file_logger.h"

static const char *TAG = "TCP_SERVER";
#define MAX_CLIENTS 5
#define JSON_BUFFER_SIZE 4096

#define MAX_REGISTERED_CLIENTS 5

typedef struct {
    char name[32];
    char ip[16];
    int cmd_port;
    int last_seen;
} client_info_t;

static client_info_t registered_clients[MAX_REGISTERED_CLIENTS];
static int client_count = 0;

// ===== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ =====
static void save_log_line(const char *line, const char *greenhouse) {
    if (!line || strlen(line) < 10) return;
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char filename[64];
    if (greenhouse && strlen(greenhouse) > 0) {
        char base[32];
        strftime(base, sizeof(base), "_%d_%m_%Y.txt", &tm_info);
        snprintf(filename, sizeof(filename), "/spiffs/%s%s", greenhouse, base);
    } else {
        strftime(filename, sizeof(filename), "/spiffs/%d_%m_%Y.txt", &tm_info);
    }
    FILE *f = fopen(filename, "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
        ESP_LOGI(TAG, "Log line saved to %s: %s", filename, line);
    } else {
        ESP_LOGE(TAG, "Failed to open log file %s", filename);
    }
}

static int parse_log_line_with_greenhouse(const char *str, char *greenhouse, size_t gh_len, char *log_line, size_t ll_len) {
    if (!str || strlen(str) < 30) return 0;
    char *semi = strchr(str, ';');
    if (!semi) return 0;
    size_t prefix_len = semi - str;
    if (prefix_len >= gh_len) prefix_len = gh_len - 1;
    strncpy(greenhouse, str, prefix_len);
    greenhouse[prefix_len] = '\0';
    const char *rest = semi + 1;
    if (strlen(rest) < 20) return 0;
    strncpy(log_line, rest, ll_len - 1);
    log_line[ll_len - 1] = '\0';
    if (!isdigit(rest[0]) || !isdigit(rest[1]) || !isdigit(rest[2]) || !isdigit(rest[3])) return 0;
    if (rest[4] != '-') return 0;
    if (!strchr(rest, ';')) return 0;
    return 1;
}

static int find_string_value(const char *json, const char *key, char *value, size_t value_len) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":\"", key);
    char *start = strstr(json, search_str);
    if (!start) return 0;
    start += strlen(search_str);
    char *end = strstr(start, "\"");
    if (!end) return 0;
    size_t len = end - start;
    if (len >= value_len) len = value_len - 1;
    strncpy(value, start, len);
    value[len] = '\0';
    return 1;
}

static int find_int_value(const char *json, const char *key, int *value) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":", key);
    char *pos = strstr(json, search_str);
    if (!pos) return 0;
    pos += strlen(search_str);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos >= '0' && *pos <= '9') {
        *value = atoi(pos);
        return 1;
    }
    return 0;
}

static int find_float_value(const char *json, const char *key, float *value) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":", key);
    char *pos = strstr(json, search_str);
    if (!pos) return 0;
    pos += strlen(search_str);
    while (*pos == ' ' || *pos == '\t') pos++;
    if ((*pos >= '0' && *pos <= '9') || *pos == '-') {
        *value = atof(pos);
        return 1;
    }
    return 0;
}

static int find_bool_detected(const char *json, const char *sensor_key, bool *detected) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":", sensor_key);
    char *pos = strstr(json, search_str);
    if (!pos) return 0;
    char *detect_pos = strstr(pos, "\"detected\":");
    if (!detect_pos) return 0;
    detect_pos += 11;
    while (*detect_pos == ' ' || *detect_pos == '\t') detect_pos++;
    if (strncmp(detect_pos, "true", 4) == 0) {
        *detected = true;
        return 1;
    } else if (strncmp(detect_pos, "false", 5) == 0) {
        *detected = false;
        return 1;
    }
    return 0;
}

static bool find_state_value(const char *json, const char *state_key) {
    char search_str[64];
    snprintf(search_str, sizeof(search_str), "\"%s\":\"", state_key);
    char *pos = strstr(json, search_str);
    if (!pos) return false;
    pos += strlen(search_str);
    if (strncmp(pos, "open", 4) == 0 || strncmp(pos, "on", 2) == 0) {
        return true;
    }
    return false;
}

// ===== ПАРСИНГ И ОБНОВЛЕНИЕ UI =====
static void parse_and_update_ui(const char *json) {
    char device_name[64] = {0};
    char time_str[32] = {0};
    find_string_value(json, "name", device_name, sizeof(device_name));
    find_string_value(json, "time", time_str, sizeof(time_str));

    int moisture_percent;
    if (!find_int_value(json, "percent", &moisture_percent)) {
        ESP_LOGW(TAG, "Moisture percent not found");
        return;
    }

    float temp_val;
    if (!find_float_value(json, "value", &temp_val)) {
        ESP_LOGW(TAG, "Temperature value not found");
        return;
    }

    bool det1 = false, det2 = false;
    find_bool_detected(json, "sensor_1", &det1);
    find_bool_detected(json, "sensor_2", &det2);

    bool valve_open = find_state_value(json, "valve");
    bool pump_on = find_state_value(json, "pump");

    int pump_speed = 0;
    find_int_value(json, "pump_speed", &pump_speed);

    ESP_LOGI(TAG, "Device: %s, Time: %s, T=%.2f, H=%d%%, S1=%d, S2=%d, valve=%s, pump=%s, speed=%d%%",
             device_name, time_str, temp_val, moisture_percent, det1, det2,
             valve_open ? "open" : "closed", pump_on ? "on" : "off", pump_speed);

    if (strcmp(device_name, "pomodoro") == 0) {
        ui_pomodoro_update_current(temp_val, moisture_percent, det1, det2, valve_open, pump_on, time_str);
        ui_pomodoro_update_speed(pump_speed);
        ui_pomodoro_reset_client_check_timer();
        file_logger_append_data("pomodoro", temp_val, moisture_percent, pump_on, valve_open);
    } else if (strcmp(device_name, "cucumber") == 0) {
        ESP_LOGI(TAG, "Cucumber data received, UI not implemented yet");
    } else {
        ESP_LOGW(TAG, "Неизвестное устройство: %s", device_name);
    }
}

// ===== ОБРАБОТКА ЗАГРУЗКИ ФАЙЛА =====
static void handle_file_upload(int sock, const char *greenhouse, const char *date_str, size_t file_size) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/spiffs/%s_%s.txt", greenhouse, date_str);

    ESP_LOGI(TAG, "Receiving file %s, size %d bytes", filename, file_size);

    FILE *f = fopen(filename, "w");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s for writing", filename);
        return;
    }

    char buffer[1024];
    size_t remaining = file_size;
    while (remaining > 0) {
        int to_read = (remaining > 1024) ? 1024 : remaining;
        int len = recv(sock, buffer, to_read, 0);
        if (len <= 0) break;
        fwrite(buffer, 1, len, f);
        remaining -= len;
    }
    fclose(f);
    ESP_LOGI(TAG, "File saved: %s (%d bytes)", filename, file_size);

    file_logger_update_chart_data_from_file(filename);
    if (strcmp(greenhouse, "pomodoro") == 0) {
        ui_pomodoro_update_chart_from_logger();
    } else if (strcmp(greenhouse, "cucumber") == 0) {
        // ui_cucumber_update_chart_from_logger();
    }
}

// ===== РЕГИСТРАЦИЯ КЛИЕНТОВ =====
static void register_client(const char *ip, int cmd_port, const char *device_name) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(registered_clients[i].ip, ip) == 0) {
            registered_clients[i].cmd_port = cmd_port;
            strncpy(registered_clients[i].name, device_name, sizeof(registered_clients[i].name)-1);
            registered_clients[i].name[sizeof(registered_clients[i].name)-1] = '\0';
            ESP_LOGI(TAG, "Updated client %s (%s) command port to %d", device_name, ip, cmd_port);
            return;
        }
    }
    if (client_count < MAX_REGISTERED_CLIENTS) {
        strncpy(registered_clients[client_count].ip, ip, sizeof(registered_clients[client_count].ip)-1);
        registered_clients[client_count].ip[sizeof(registered_clients[client_count].ip)-1] = '\0';
        registered_clients[client_count].cmd_port = cmd_port;
        strncpy(registered_clients[client_count].name, device_name, sizeof(registered_clients[client_count].name)-1);
        registered_clients[client_count].name[sizeof(registered_clients[client_count].name)-1] = '\0';
        client_count++;
        ESP_LOGI(TAG, "Registered new client %s (%s) command port %d", device_name, ip, cmd_port);
    } else {
        ESP_LOGW(TAG, "Too many clients, cannot register %s", device_name);
    }
}

static void update_client_name(const char *ip, const char *device_name) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(registered_clients[i].ip, ip) == 0) {
            strncpy(registered_clients[i].name, device_name, sizeof(registered_clients[i].name)-1);
            registered_clients[i].name[sizeof(registered_clients[i].name)-1] = '\0';
            ESP_LOGI(TAG, "Updated client %s name to %s (port %d)", ip, device_name, registered_clients[i].cmd_port);
            return;
        }
    }
    ESP_LOGW(TAG, "Client with IP %s not found for name update", ip);
}

// ===== ОБРАБОТКА КЛИЕНТА (TCP) =====
static void handle_client(int sock) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(sock, (struct sockaddr *)&client_addr, &addr_len) != 0) {
        ESP_LOGE(TAG, "Cannot get client address");
        close(sock);
        return;
    }
    char client_ip[16];
    inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip)-1);

    char rx_buffer[256];
    int len;
    char json_buffer[JSON_BUFFER_SIZE] = {0};
    int json_len = 0;

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%d.%m.%Y %H:%M:%S", &tm_info);
    send(sock, time_buf, strlen(time_buf), 0);
    send(sock, "\n", 1, 0);
    ESP_LOGI(TAG, "Направлено время клиенту %s: %s", client_ip, time_buf);

    while (1) {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            if (errno == 104) {
                ESP_LOGW(TAG, "Клиент %s разорвал соединение (errno 104)", client_ip);
            } else {
                ESP_LOGE(TAG, "Ошибка приёма от %s: errno %d", client_ip, errno);
            }
            break;
        } else if (len == 0) {
            ESP_LOGI(TAG, "Клиент %s закрыл соединение", client_ip);
            break;
        }

        rx_buffer[len] = '\0';
        ESP_LOGI(TAG, "Получено %d байт от %s", len, client_ip);

        if (strncmp(rx_buffer, "CMD_PORT:", 9) == 0) {
            int cmd_port = atoi(rx_buffer + 9);
            register_client(client_ip, cmd_port, "unknown");
            continue;
        }

        if (strncmp(rx_buffer, "FILE:", 5) == 0) {
            char greenhouse[32], date_str[32];
            long size;
            if (sscanf(rx_buffer, "FILE:%[^:]:%[^:]:%ld", greenhouse, date_str, &size) == 3) {
                handle_file_upload(sock, greenhouse, date_str, size);
                update_client_name(client_ip, greenhouse);
                break;
            } else {
                ESP_LOGE(TAG, "Invalid FILE header format");
                break;
            }
        }

        if (json_len + len < JSON_BUFFER_SIZE - 1) {
            memcpy(json_buffer + json_len, rx_buffer, len);
            json_len += len;
            json_buffer[json_len] = '\0';
        } else {
            ESP_LOGW(TAG, "Буфер переполнен, сброс");
            json_len = 0;
            json_buffer[0] = '\0';
            continue;
        }

        int i = json_len - 1;
        while (i >= 0 && (json_buffer[i] == ' ' || json_buffer[i] == '\n' || json_buffer[i] == '\r')) i--;
        if (i >= 0 && json_buffer[i] == '}') {
            char *first_brace = strchr(json_buffer, '{');
            if (first_brace) {
                parse_and_update_ui(first_brace);
                char dev_name[64];
                if (find_string_value(first_brace, "name", dev_name, sizeof(dev_name))) {
                    update_client_name(client_ip, dev_name);
                }
            }
            json_len = 0;
            json_buffer[0] = '\0';
        } else if (json_len > 20 && strchr(json_buffer, ';')) {
            char *line = json_buffer;
            char *next;
            while ((next = strchr(line, '\n')) != NULL) {
                *next = '\0';
                if (strlen(line) > 10) {
                    char greenhouse[32];
                    char log_line[256];
                    if (parse_log_line_with_greenhouse(line, greenhouse, sizeof(greenhouse), log_line, sizeof(log_line))) {
                        save_log_line(log_line, greenhouse);
                        update_client_name(client_ip, greenhouse);
                    } else {
                        save_log_line(line, NULL);
                    }
                }
                line = next + 1;
            }
            if (strlen(line) > 10) {
                char greenhouse[32];
                char log_line[256];
                if (parse_log_line_with_greenhouse(line, greenhouse, sizeof(greenhouse), log_line, sizeof(log_line))) {
                    save_log_line(log_line, greenhouse);
                    update_client_name(client_ip, greenhouse);
                } else {
                    save_log_line(line, NULL);
                }
            }
            json_len = 0;
            json_buffer[0] = '\0';
        }
    }

    shutdown(sock, 0);
    close(sock);
}

static void handle_client_wrapper(void *pvParameters) {
    int sock = (intptr_t)pvParameters;
    handle_client(sock);
    vTaskDelete(NULL);
}

// ===== СЕРВЕРНАЯ ЗАДАЧА =====
static void tcp_server_task(void *pvParameters) {
    uint16_t port = *((uint16_t*)pvParameters);
    free(pvParameters);

    int listen_sock = -1;
    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    while (1) {
        if (listen_sock < 0) {
            listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (listen_sock < 0) {
                ESP_LOGE(TAG, "Не удалось создать сокет: errno %d", errno);
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            int opt = 1;
            setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
                ESP_LOGE(TAG, "Ошибка bind: errno %d", errno);
                close(listen_sock);
                listen_sock = -1;
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            if (listen(listen_sock, MAX_CLIENTS) != 0) {
                ESP_LOGE(TAG, "Ошибка listen: errno %d", errno);
                close(listen_sock);
                listen_sock = -1;
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            ESP_LOGI(TAG, "TCP-сервер запущен на порту %d", port);
        }

        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (client_sock < 0) {
            if (errno == 23) {
                ESP_LOGW(TAG, "Accept error (errno 23) - too many open files, closing listen socket");
                close(listen_sock);
                listen_sock = -1;
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            } else {
                ESP_LOGE(TAG, "Ошибка accept: errno %d", errno);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        char addr_str[16];
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Новое подключение от %s:%d", addr_str, source_addr.sin_port);
        xTaskCreate(handle_client_wrapper, "tcp_client", 8192, (void*)(intptr_t)client_sock, 5, NULL);
    }
}

// ===== ПУБЛИЧНЫЕ ФУНКЦИИ =====
void tcp_server_start(uint16_t port) {
    uint16_t *port_ptr = malloc(sizeof(uint16_t));
    *port_ptr = port;
    xTaskCreate(tcp_server_task, "tcp_server", 8192, port_ptr, 5, NULL);
}

bool tcp_is_client_connected(const char *name) {
    if (!name) return false;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(registered_clients[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

void tcp_send_command_to_ip(const char *ip, uint16_t port, const char *cmd) {
    if (!ip || !cmd) {
        ESP_LOGE(TAG, "Invalid parameters");
        return;
    }
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket for command to %s:%d", ip, port);
        return;
    }
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &dest_addr.sin_addr) != 1) {
        ESP_LOGE(TAG, "Invalid IP: %s", ip);
        close(sock);
        return;
    }
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Cannot connect to %s:%d", ip, port);
        close(sock);
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    int ret = send(sock, buf, strlen(buf), 0);
    if (ret > 0) {
        ESP_LOGI(TAG, "Command '%s' sent to %s:%d", cmd, ip, port);
        char resp[32];
        int len = recv(sock, resp, sizeof(resp) - 1, 1000);
        if (len > 0) {
            resp[len] = '\0';
            ESP_LOGI(TAG, "Response from %s:%d: %s", ip, port, resp);
            // Обработка ошибок
            if (strncmp(resp, "ERROR", 5) == 0) {
                if (strcmp(cmd, "pump_on") == 0) {
                    file_logger_log_event("pomodoro", "pump", "off");
                } else if (strcmp(cmd, "valve_open") == 0) {
                    file_logger_log_event("pomodoro", "valve", "off");
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to send command to %s:%d", ip, port);
    }
    shutdown(sock, 0);
    close(sock);
}

void tcp_send_command(const char *client_name, const char *cmd) {
    ESP_LOGI(TAG, "tcp_send_command called for client '%s' with cmd '%s'", client_name, cmd);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(registered_clients[i].name, client_name) == 0) {
            int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to create socket for command to %s", client_name);
                return;
            }
            struct sockaddr_in dest_addr;
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(registered_clients[i].cmd_port);
            inet_pton(AF_INET, registered_clients[i].ip, &dest_addr.sin_addr);

            if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
                ESP_LOGE(TAG, "Cannot connect to %s (%s:%d)", client_name, registered_clients[i].ip, registered_clients[i].cmd_port);
                close(sock);
                return;
            }

            char buf[64];
            snprintf(buf, sizeof(buf), "%s\n", cmd);
            int ret = send(sock, buf, strlen(buf), 0);
            if (ret > 0) {
                ESP_LOGI(TAG, "Command '%s' sent to %s (%s:%d)", cmd, client_name, registered_clients[i].ip, registered_clients[i].cmd_port);
                char resp[32];
                int len = recv(sock, resp, sizeof(resp) - 1, 1000);
                if (len > 0) {
                    resp[len] = '\0';
                    ESP_LOGI(TAG, "Response from %s: %s", client_name, resp);
                }
            } else {
                ESP_LOGE(TAG, "Failed to send command to %s", client_name);
            }
            shutdown(sock, 0);
            close(sock);
            return;
        }
    }
    ESP_LOGW(TAG, "Client %s not registered", client_name);
}