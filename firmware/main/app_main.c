#include "esp_err.h"
#include "esp_log.h"
#include "board_port.h"
#include "mode_router.h"
#include "ble_hid.h"
#include "host_link.h"
#include "pmu_axp2101.h"
#include "hw_buttons.h"
#include "button_actions.h"

static const char *TAG = "round_remote";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting round-screen remote on Waveshare AMOLED 1.75 family BSP");
    /* BLE first: the controller needs internal RAM before LVGL/display buffers take it.
     * One shared BLE HID device, so the upper key can play/pause from any screen. */
    esp_err_t err = ble_hid_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "BLE HID start failed: %s", esp_err_to_name(err));

    ESP_ERROR_CHECK(board_port_init());
    ESP_ERROR_CHECK(mode_router_init());

    /* Now-playing metadata source (stub until the Wi-Fi bridge lands). */
    err = host_link_start(NULL);
    if (err != ESP_OK) ESP_LOGW(TAG, "Host link start failed: %s", esp_err_to_name(err));

    err = pmu_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "AXP2101 unavailable; upper key and power-off disabled (%s)", esp_err_to_name(err));

    ESP_ERROR_CHECK(hw_buttons_start(button_actions_handle));
    ESP_LOGI(TAG, "Ready");
}
