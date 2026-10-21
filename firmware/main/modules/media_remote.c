#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "media_remote.h"
#include "media_link.h"

static const char *TAG = "media_remote";
static bool s_active;

esp_err_t media_remote_enter(void)
{
    s_active = true;
    esp_err_t err = media_link_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Media transport is not connected yet (%s)", esp_err_to_name(err));
    }
    return ESP_OK;
}

void media_remote_exit(void)
{
    if (s_active) {
        media_link_stop();
        s_active = false;
    }
}

esp_err_t media_remote_send(media_command_t command)
{
    if (!s_active) {
        return ESP_ERR_INVALID_STATE;
    }
    return media_link_send_command(command);
}
