#include "esp_log.h"
#include "esp_err.h"
#include "ble_hid.h"

static const char *TAG = "ble_hid";

esp_err_t ble_hid_start(void)
{
    ESP_LOGW(TAG, "BLE HID stack/profile switching not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

void ble_hid_stop(void)
{
}

esp_err_t ble_hid_set_pressed(bool pressed)
{
    ESP_LOGW(TAG, "PTT key %s requested; HID adapter is a stub", pressed ? "down" : "up");
    return ESP_ERR_NOT_SUPPORTED;
}
