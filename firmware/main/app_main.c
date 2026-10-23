#include "esp_err.h"
#include "esp_log.h"
#include "board_port.h"
#include "mode_router.h"

static const char *TAG = "round_remote";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting round-screen remote on Waveshare AMOLED 1.75 family BSP");
    ESP_ERROR_CHECK(board_port_init());
    ESP_ERROR_CHECK(mode_router_init());
    ESP_LOGI(TAG, "Display/touch ready; BLE HID consumer controls available after host pairing; Wi-Fi bridge and speech backend remain unimplemented");
}
