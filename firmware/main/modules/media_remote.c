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

esp_err_t media_remote_command(media_command_t command)
{
    esp_err_t err = media_link_send_command(command);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Media command %d failed: %s", (int)command, esp_err_to_name(err));
    }
    return err;
}

esp_err_t media_remote_play_pause(void)  { return media_remote_command(MEDIA_COMMAND_PLAY_PAUSE); }
esp_err_t media_remote_previous(void)    { return media_remote_command(MEDIA_COMMAND_PREVIOUS); }
esp_err_t media_remote_next(void)        { return media_remote_command(MEDIA_COMMAND_NEXT); }
esp_err_t media_remote_volume_up(void)   { return media_remote_command(MEDIA_COMMAND_VOLUME_UP); }
esp_err_t media_remote_volume_down(void) { return media_remote_command(MEDIA_COMMAND_VOLUME_DOWN); }
esp_err_t media_remote_mute(void)        { return media_remote_command(MEDIA_COMMAND_MUTE); }
esp_err_t media_remote_stop(void)        { return media_remote_command(MEDIA_COMMAND_STOP); }

bool media_remote_is_connected(void)
{
    return media_link_is_connected();
}
