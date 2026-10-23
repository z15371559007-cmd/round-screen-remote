#include <stdbool.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "mode_router.h"
#include "board_port.h"
#include "media_remote.h"
#include "push_to_talk.h"
#include "pet_standby.h"
#include "badge_tools.h"

static const char *TAG = "mode_router";
static app_mode_t s_mode = APP_MODE_HOME;
static bool s_initialized;

const char *app_mode_name(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: return "standby_pet";
    case APP_MODE_MEDIA_REMOTE: return "media_remote";
    case APP_MODE_PUSH_TO_TALK: return "push_to_talk";
    case APP_MODE_HOME: return "home";
    case APP_MODE_BADGE_TOOLS: return "badge_tools";
    default: return "invalid";
    }
}

static void leave_mode(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: pet_standby_exit(); break;
    case APP_MODE_MEDIA_REMOTE: media_remote_exit(); break;
    case APP_MODE_PUSH_TO_TALK: push_to_talk_exit(); break;
    case APP_MODE_BADGE_TOOLS: badge_tools_exit((uint64_t)esp_timer_get_time()); break;
    case APP_MODE_HOME: break;
    default: break;
    }
}

static esp_err_t enter_mode(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_STANDBY_PET: return pet_standby_enter();
    case APP_MODE_MEDIA_REMOTE: return media_remote_enter();
    case APP_MODE_PUSH_TO_TALK: return push_to_talk_enter();
    case APP_MODE_BADGE_TOOLS: badge_tools_enter(); return ESP_OK;
    case APP_MODE_HOME: return ESP_OK;
    default: return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t mode_router_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_initialized = true;
    s_mode = APP_MODE_HOME;
    board_port_render_mode(s_mode);
    ESP_LOGI(TAG, "Entering %s", app_mode_name(s_mode));
    return enter_mode(s_mode);
}

esp_err_t mode_router_switch_to(app_mode_t mode)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mode < APP_MODE_STANDBY_PET || mode > APP_MODE_BADGE_TOOLS) {
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
