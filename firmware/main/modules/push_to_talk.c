#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "push_to_talk.h"
#include "ble_hid.h"

static const char *TAG = "push_to_talk";
static bool s_active;

esp_err_t push_to_talk_enter(void)
{
    s_active = true;
    esp_err_t err = ble_hid_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BLE HID adapter is not connected yet (%s)", esp_err_to_name(err));
    }
    return ESP_OK;
}

void push_to_talk_exit(void)
{
    if (s_active) {
        (void)ble_hid_set_pressed(false);
        ble_hid_stop();
        s_active = false;
    }
}

esp_err_t push_to_talk_on_pressed(void)
{
    if (!s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    return ble_hid_set_pressed(true);
}

esp_err_t push_to_talk_on_released(void)
{
    if (!s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    return ble_hid_set_pressed(false);
}
