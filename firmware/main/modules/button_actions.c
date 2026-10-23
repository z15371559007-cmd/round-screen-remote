#include "button_actions.h"

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_port.h"
#include "mode_router.h"
#include "push_to_talk.h"
#include "media_remote.h"
#include "pmu_axp2101.h"

static const char *TAG = "button_actions";
static bool s_ptt_from_key;   /* the current PWR hold started a PTT key-down */

static void release_ptt_if_held(void)
{
    if (s_ptt_from_key) {
        (void)push_to_talk_release(PTT_SOURCE_KEY);
        board_port_set_listening(false);
        s_ptt_from_key = false;
    }
}

void button_actions_handle(hw_button_event_t event)
{
    if (event == HW_BUTTON_BOOT_LONG) {
        board_port_ui_lock();
        release_ptt_if_held();
        push_to_talk_release_all();   /* also drops a touch-held PTT before power-off */
        board_port_show_power_off();
        board_port_ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(900));
        esp_err_t err = pmu_power_off();
        ESP_LOGE(TAG, "Power-off failed: %s", esp_err_to_name(err));
        return;
    }

    board_port_ui_lock();
    const app_mode_t mode = mode_router_current();
    esp_err_t err = ESP_OK;

    switch (event) {
    case HW_BUTTON_BOOT_SHORT:
        release_ptt_if_held();
        err = mode_router_switch_to(APP_MODE_HOME);
        break;
    case HW_BUTTON_PWR_DOWN:
        if (mode == APP_MODE_PUSH_TO_TALK) {
            err = push_to_talk_press(PTT_SOURCE_KEY);
            s_ptt_from_key = (err == ESP_OK);
            if (s_ptt_from_key) board_port_set_listening(true);
            else board_port_set_ptt_failed(true);   /* screen shows why nothing was sent */
            ESP_LOGI(TAG, "PWR PTT down -> %s", esp_err_to_name(err));
        }
        break;
    case HW_BUTTON_PWR_UP:
        if (s_ptt_from_key) {
            release_ptt_if_held();
        } else if (mode == APP_MODE_PUSH_TO_TALK) {
            board_port_set_ptt_failed(false);
        } else if (mode != APP_MODE_PUSH_TO_TALK && mode != APP_MODE_BADGE_TOOLS) {
            err = media_remote_play_pause();   /* single click = play/pause */
        }
        break;
    default:
        break;
    }
    board_port_ui_unlock();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Key action unavailable: %s", esp_err_to_name(err));
    }
}
