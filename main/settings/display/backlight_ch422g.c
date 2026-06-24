#include "backlight_ch422g.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings/touch/touch.h"

static const char *TAG = "BACKLIGHT_CH422G";

#define CH422G_ADDR                0x24
#define BACKLIGHT_IO               0      // IO0 – подсветка (LOW = ON)
#define TP_RST_IO                  2      // IO2 – сброс тачскрина (активный LOW)

static i2c_master_dev_handle_t dev_handle = NULL;
static uint8_t ch422g_state = 0xFF;

// ---- Вспомогательные функции I2C ----
static esp_err_t ch422g_write(uint8_t data)
{
    if (dev_handle == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t buf[1] = { data };
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t ch422g_set_io(int io, bool high)
{
    if (io < 0 || io > 7) return ESP_ERR_INVALID_ARG;
    if (high) {
        ch422g_state |= (1 << io);
    } else {
        ch422g_state &= ~(1 << io);
    }
    return ch422g_write(ch422g_state);
}

static esp_err_t ch422g_reinit_device(void)
{
    if (g_i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized!");
        return ESP_ERR_INVALID_STATE;
    }
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_ADDR,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(g_i2c_bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-add CH422G: %d", err);
        return err;
    }
    ESP_LOGI(TAG, "CH422G device re-created");
    return ESP_OK;
}

// ---- Сброс тачскрина через IO2 (TP_RST) ----
void backlight_ch422g_touch_reset(void)
{
    ESP_LOGI(TAG, "Resetting touch controller via TP_RST (IO2)");
    esp_err_t err = ch422g_set_io(TP_RST_IO, false);  // LOW
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TP_RST LOW, error %d, reinit device", err);
        ch422g_reinit_device();
        ch422g_set_io(TP_RST_IO, false);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    err = ch422g_set_io(TP_RST_IO, true);   // HIGH
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TP_RST HIGH, error %d, reinit device", err);
        ch422g_reinit_device();
        ch422g_set_io(TP_RST_IO, true);
    }
    vTaskDelay(pdMS_TO_TICKS(150));
}

// ---- Инициализация CH422G ----
esp_err_t backlight_ch422g_init(void)
{
    if (g_i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized! Call touch_init first.");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(g_i2c_bus_handle, &dev_config, &dev_handle), TAG, "Device add error");

    esp_err_t err = i2c_master_probe(g_i2c_bus_handle, CH422G_ADDR, pdMS_TO_TICKS(200));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CH422G not responding at 0x%02X", CH422G_ADDR);
        return err;
    }
    ESP_LOGI(TAG, "CH422G found at 0x%02X", CH422G_ADDR);

    // Начальное состояние: все выходы HIGH (подсветка выключена)
    ch422g_state = 0xFF;
    ESP_ERROR_CHECK(ch422g_write(ch422g_state));
    ESP_LOGI(TAG, "Initial state: 0x%02X (backlight OFF)", ch422g_state);

    // Сброс тачскрина при старте
    backlight_ch422g_touch_reset();
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

// ---- Управление подсветкой (простейшее включение/выключение) ----
esp_err_t backlight_ch422g_set(bool on)
{
    ESP_LOGI(TAG, "backlight_ch422g_set(%d) called", on);
    // Подсветка: LOW = включена, HIGH = выключена
    esp_err_t err = ch422g_set_io(BACKLIGHT_IO, !on);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ch422g_set_io failed: %d, reinitializing device", err);
        ch422g_reinit_device();
        err = ch422g_set_io(BACKLIGHT_IO, !on);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Retry failed: %d", err);
            return err;
        }
    }
    ESP_LOGI(TAG, "backlight_ch422g_set(%d) done, new state 0x%02X", on, ch422g_state);
    return ESP_OK;
}