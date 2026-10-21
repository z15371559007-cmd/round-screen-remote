#include "esp_log.h"
#include "esp_err.h"
#include "media_link.h"

static const char *TAG = "media_link";

esp_err_t media_link_start(void)
{
    ESP_LOGW(TAG, "WebSocket client not implemented; see docs/architecture.md");
    return ESP_ERR_NOT_SUPPORTED;
}

void media_link_stop(void)
{
}

esp_err_t media_link_send_command(media_command_t command)
{
    (void)command;
    return ESP_ERR_NOT_SUPPORTED;
}
