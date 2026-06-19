#include "file_logger.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

static const char *TAG = "FILE_LOGGER";
static const char *BASE_PATH = "/spiffs";

#define CHART_POINTS 144
static int32_t chart_temp[CHART_POINTS] = {0};
static int32_t chart_humi[CHART_POINTS] = {0};
static int chart_point_count = 0;

static bool parse_log_line_flexible(const char *line, float *temperature, int *moisture,
                                    bool *sensor1, bool *sensor2) {
    const char *ptr = line;
    const char *semi = strchr(ptr, ';');
    if (!semi) return false;
    ptr = semi + 1;

    char date[12], time[9];
    float moist_float, temp;
    int s1, s2;
    int n = sscanf(ptr, "%11s %8s;%f;%f;%d;%d",
                   date, time, &moist_float, &temp, &s1, &s2);
    if (n == 6) {
        *moisture = (int)moist_float;
        *temperature = temp;
        *sensor1 = (s1 == 1);
        *sensor2 = (s2 == 1);
        return true;
    }
    return false;
}

void file_logger_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted. Total: %d, Used: %d", total, used);
    }
}

bool file_logger_get_latest_data(const char* greenhouse,
                                 float* temperature, int* moisture,
                                 bool* sensor1_detected, bool* sensor2_detected) {
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
    ESP_LOGD(TAG, "Reading log file: %s", filename);

    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Log file not found: %s", filename);
        return false;
    }

    char line[128];
    char *last_valid_line = NULL;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            if (last_valid_line) free(last_valid_line);
            last_valid_line = malloc(strlen(line) + 1);
            if (last_valid_line) strcpy(last_valid_line, line);
        }
    }
    fclose(f);

    if (last_valid_line == NULL) {
        ESP_LOGW(TAG, "No data lines found in file");
        return false;
    }

    bool success = parse_log_line_flexible(last_valid_line, temperature, moisture,
                                           sensor1_detected, sensor2_detected);
    free(last_valid_line);
    return success;
}

void file_logger_cleanup_old_logs(int days_keep) {
    DIR *dir = opendir(BASE_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory %s: %s", BASE_PATH, strerror(errno));
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strncmp(name, "pomodoro_", 9) != 0) continue;
        size_t len = strlen(name);
        if (len < 5 || strcmp(name + len - 4, ".txt") != 0) continue;

        int day, month, year;
        if (sscanf(name, "pomodoro_%d_%d_%d.txt", &day, &month, &year) != 3) {
            ESP_LOGW(TAG, "Skipping file with unexpected name: %s", name);
            continue;
        }
        if (year < 2000 || year > 2100) {
            ESP_LOGW(TAG, "Invalid year in file name: %s", name);
            continue;
        }

        struct tm tm_file = {
            .tm_year = year - 1900,
            .tm_mon = month - 1,
            .tm_mday = day,
            .tm_hour = 0,
            .tm_min = 0,
            .tm_sec = 0,
            .tm_isdst = -1,
        };
        time_t file_time = mktime(&tm_file);
        if (file_time == -1) {
            ESP_LOGW(TAG, "Failed to convert file date for %s", name);
            continue;
        }

        double diff_days = difftime(now, file_time) / (60 * 60 * 24);
        if (diff_days > days_keep) {
            char full_path[512];
            int written = snprintf(full_path, sizeof(full_path), "%s/%s", BASE_PATH, name);
            if (written >= (int)sizeof(full_path)) {
                ESP_LOGW(TAG, "Path truncated: %s/%s", BASE_PATH, name);
                continue;
            }
            if (unlink(full_path) == 0) {
                ESP_LOGI(TAG, "Deleted old log file: %s (age %.1f days)", full_path, diff_days);
            } else {
                ESP_LOGE(TAG, "Failed to delete %s: %s", full_path, strerror(errno));
            }
        }
    }
    closedir(dir);
}

static int time_to_index(const char *time_str) {
    int hour, minute, second;
    if (sscanf(time_str, "%d:%d:%d", &hour, &minute, &second) != 3) return -1;
    int minutes_since_midnight = hour * 60 + minute;
    int index = minutes_since_midnight / 10;
    if (index < 0) index = 0;
    if (index >= CHART_POINTS) index = CHART_POINTS - 1;
    return index;
}

void file_logger_update_chart_data(void) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char filename[64];
    strftime(filename, sizeof(filename), "/spiffs/pomodoro_%d_%m_%Y.txt", &tm_info);
    file_logger_update_chart_data_from_file(filename);
}

void file_logger_update_chart_data_from_file(const char *filename) {
    ESP_LOGI(TAG, "Updating chart data from file: %s", filename);

    memset(chart_temp, 0, sizeof(chart_temp));
    memset(chart_humi, 0, sizeof(chart_humi));
    chart_point_count = 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        float temp;
        int moisture;
        bool s1, s2;
        if (parse_log_line_flexible(line, &temp, &moisture, &s1, &s2)) {
            const char *ptr = line;
            const char *semi = strchr(ptr, ';');
            if (semi) ptr = semi + 1;
            char time_str[9];
            if (sscanf(ptr, "%*s %8s", time_str) == 1) {
                int idx = time_to_index(time_str);
                if (idx >= 0 && idx < CHART_POINTS) {
                    chart_temp[idx] = (int32_t)(temp * 10);
                    chart_humi[idx] = moisture;
                    chart_point_count++;
                }
            }
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "Chart data updated: %d valid points", chart_point_count);
}

void file_logger_get_chart_data(int32_t **temp_array, int32_t **humi_array, int *point_count) {
    *temp_array = chart_temp;
    *humi_array = chart_humi;
    *point_count = chart_point_count;
}