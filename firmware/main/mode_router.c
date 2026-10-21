#include <stdbool.h>
#include "esp_log.h"
#include "mode_router.h"
#include "board_port.h"
#include "media_remote.h"
#include "push_to_talk.h"
#include "pet_standby.h"

static const char *TAG = "mode_router";
static app_mode_t s_mode = APP_MODE_STANDBY_PET;
static bool s_initialized;

const char *app_mode_name(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: return "standby_pet";
    case APP_MODE_MEDIA_REMOTE: return "media_remote";
    case APP_MODE_PUSH_TO_TALK: return "push_to_talk";
    default: return "invalid";
    }
}

static void leave_mode(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: pet_standby_exit(); break;
    case APP_MODE_MEDIA_REMOTE: media_remote_exit(); break;
    case APP_MODE_PUSH_TO_TALK: push_to_talk_exit(); break;
    default: break;
    }
}

static esp_err_t enter_mode(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: return pet_standby_enter();
    case APP_MODE_MEDIA_REMOTE: return media_remote_enter();
    case APP_MODE_PUSH_TO_TALK: return push_to_talk_enter();
    default: return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t mode_router_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_initialized = true;
    s_mode = APP_MODE_STANDBY_PET;
    board_port_render_mode(s_mode);
    ESP_LOGI(TAG, "Entering %s", app_mode_name(s_mode));
    return enter_mode(s_mode);
}

esp_err_t mode_router_switch_to(app_mode_t mode)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mode < APP_MODE_STANDBY_PET || mode > APP_MODE_PUSH_TO_TALK) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mode == s_mode) {
        return ESP_OK;
    }

    leave_mode(s_mode);
    s_mode = mode;
    board_port_render_mode(s_mode);
    ESP_LOGI(TAG, "Entering %s", app_mode_name(s_mode));
    return enter_mode(s_mode);
}

app_mode_t mode_router_current(void)
{
    return s_mode;
}
