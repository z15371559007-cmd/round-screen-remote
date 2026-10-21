#include "esp_err.h"
#include "esp_log.h"
#include "board_port.h"
#include "mode_router.h"

static const char *TAG = "round_remote";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting round-screen remote scaffold");
    ESP_ERROR_CHECK(board_port_init());
    ESP_ERROR_CHECK(mode_router_init());
    ESP_LOGI(TAG, "Default mode: standby pet; hardware adapters are stubs");
}
