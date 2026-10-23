#include <stdbool.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include "board_port.h"
#include "mode_router.h"
#include "media_remote.h"
#include "push_to_talk.h"
#include "ble_hid.h"

static const char *TAG = "board_waveshare";
static volatile app_mode_t s_requested_mode = APP_MODE_MEDIA_REMOTE;
static volatile bool s_listening;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_main_button_label;
static lv_obj_t *s_main_button;
static lv_obj_t *s_media_controls;

static void set_label(lv_obj_t *label, const char *text)
{
    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

/* Called by the BSP's LVGL task, so UI mutations remain on the LVGL thread. */
static void refresh_ui(lv_timer_t *timer)
{
    (void)timer;
    const app_mode_t mode = s_requested_mode;
    if (mode == APP_MODE_PUSH_TO_TALK) {
        set_label(s_mode_label, "PUSH TO TALK");
        set_label(s_main_button_label, "按住说话");
        set_label(s_state_label, s_listening ? "PTT 键已按下" :
                   !ble_hid_is_connected() ? "BLE 未连接" :
                   !ble_hid_ptt_key_configured() ? "PTT 键位未配置" : "按住说话");
        lv_obj_add_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_main_button, lv_color_hex(s_listening ? 0x6D2418 : 0x211712), 0);
    } else if (mode == APP_MODE_MEDIA_REMOTE) {
        set_label(s_mode_label, "媒体遥控");
        set_label(s_main_button_label, "播放 / 暂停");
        set_label(s_state_label, ble_hid_is_connected() ? "BLE 已连接" : "BLE 未连接");
        lv_obj_clear_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_main_button, lv_color_hex(0x211712), 0);
    } else {
        lv_obj_add_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
        set_label(s_mode_label, "圆屏遥控器");
        set_label(s_main_button_label, "选择模式");
        set_label(s_state_label, "触按下方切换");
        lv_obj_set_style_bg_color(s_main_button, lv_color_hex(0x211712), 0);
    }
}

static void mode_button_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const app_mode_t mode = (app_mode_t)(intptr_t)lv_event_get_user_data(event);
    esp_err_t err = mode_router_switch_to(mode);
    if (err != ESP_OK) ESP_LOGW(TAG, "Mode change failed: %s", esp_err_to_name(err));
}

static void main_button_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    const app_mode_t mode = s_requested_mode;
    esp_err_t err = ESP_OK;

    if (mode == APP_MODE_PUSH_TO_TALK) {
        if (code == LV_EVENT_PRESSED) {
            err = push_to_talk_on_pressed();
            s_listening = (err == ESP_OK);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            err = push_to_talk_on_released();
            s_listening = false;
        }
    } else if (mode == APP_MODE_MEDIA_REMOTE && code == LV_EVENT_CLICKED) {
        err = media_remote_send(MEDIA_COMMAND_PLAY_PAUSE);
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Control unavailable: %s", esp_err_to_name(err));
    }
}

static void media_control_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    media_command_t command = (media_command_t)(intptr_t)lv_event_get_user_data(event);
    esp_err_t err = media_remote_send(command);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Media control unavailable: %s", esp_err_to_name(err));
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int32_t width, int32_t height)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x211712), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xA45336), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xF1E7DC), 0);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

esp_err_t board_port_init(void)
{
    lv_display_t *display = bsp_display_start();
    ESP_RETURN_ON_FALSE(display != NULL, ESP_FAIL, TAG, "Waveshare BSP display/touch init failed");
    ESP_RETURN_ON_ERROR(bsp_display_lock(-1), TAG, "LVGL lock failed");

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B0B0B), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 20, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_mode_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_mode_label, lv_color_hex(0xF1E7DC), 0);
    lv_obj_set_style_text_font(s_mode_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(s_mode_label, "圆屏遥控器");

    s_main_button = make_button(screen, "选择模式", 228, 228);
    lv_obj_set_style_radius(s_main_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_main_button, 3, 0);
    lv_obj_add_event_cb(s_main_button, main_button_event, LV_EVENT_ALL, NULL);
    s_main_button_label = lv_obj_get_child(s_main_button, 0);
    lv_obj_set_style_text_font(s_main_button_label, &lv_font_montserrat_24, 0);

    s_state_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_state_label, lv_color_hex(0xC88B6C), 0);
    lv_label_set_text(s_state_label, "触按下方切换");

    s_media_controls = lv_obj_create(screen);
    lv_obj_set_width(s_media_controls, LV_PCT(100));
    lv_obj_set_height(s_media_controls, 42);
    lv_obj_set_style_bg_opa(s_media_controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_media_controls, 0, 0);
    lv_obj_set_style_pad_all(s_media_controls, 0, 0);
    lv_obj_set_style_pad_column(s_media_controls, 8, 0);
    lv_obj_set_flex_flow(s_media_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_media_controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static const struct { const char *icon; media_command_t command; } controls[] = {
        { LV_SYMBOL_PREV, MEDIA_COMMAND_PREVIOUS },
        { LV_SYMBOL_MINUS, MEDIA_COMMAND_VOLUME_DOWN },
        { LV_SYMBOL_PLUS, MEDIA_COMMAND_VOLUME_UP },
        { LV_SYMBOL_NEXT, MEDIA_COMMAND_NEXT },
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        lv_obj_t *button = make_button(s_media_controls, controls[i].icon, 86, 38);
        lv_obj_set_style_radius(button, 12, 0);
        lv_obj_add_event_cb(button, media_control_event, LV_EVENT_CLICKED,
                            (void *)(intptr_t)controls[i].command);
    }
    lv_obj_add_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 76);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *media_button = make_button(row, "媒体", 184, 64);
    lv_obj_add_event_cb(media_button, mode_button_event, LV_EVENT_CLICKED, (void *)(intptr_t)APP_MODE_MEDIA_REMOTE);
    lv_obj_t *ptt_button = make_button(row, "PTT", 184, 64);
    lv_obj_add_event_cb(ptt_button, mode_button_event, LV_EVENT_CLICKED, (void *)(intptr_t)APP_MODE_PUSH_TO_TALK);

    lv_timer_create(refresh_ui, 150, NULL);
    bsp_display_unlock();
    ESP_LOGI(TAG, "Waveshare family BSP display and CST9217 touch initialized (466x466)");
    return ESP_OK;
}

void board_port_render_mode(app_mode_t mode)
{
    s_requested_mode = mode;
    s_listening = false;
}

void board_port_show_pet_expression(pet_expression_t expression)
{
    (void)expression;
}

void board_port_set_listening(bool listening)
{
    s_listening = listening;
}
