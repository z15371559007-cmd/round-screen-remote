#include <stdbool.h>
#include <math.h>
#include <stdint.h>

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
LV_FONT_DECLARE(rsr_font_cjk_24);
#define UI_FONT (&rsr_font_cjk_24)

#define FACE_SIZE       466
#define FACE_CENTER     233
#define TICK_COUNT      60
#define COLOR_BG        0x080909
#define COLOR_DIAL      0x171819
#define COLOR_TICK      0x39302D
#define COLOR_AMBER     0xF15B3D
#define COLOR_TEXT      0xF2EEE9
#define COLOR_MUTED     0x928B85
#define COLOR_BUTTON    0x171310

static volatile app_mode_t s_requested_mode = APP_MODE_HOME;
static volatile bool s_listening;
static volatile bool s_powering_off;
static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_state;
static lv_obj_t *s_center_button;
static lv_obj_t *s_center_label;
static lv_obj_t *s_mode_controls;
static lv_obj_t *s_media_controls;
static lv_obj_t *s_prev_button;
static lv_obj_t *s_next_button;
static lv_obj_t *s_bottom_note;
static lv_obj_t *s_ticks[TICK_COUNT];

static void set_label(lv_obj_t *label, const char *text)
{
    if (label != NULL) lv_label_set_text(label, text);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            uint32_t color, int32_t width, int32_t height)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    if (width > 0) lv_obj_set_width(label, width);
    if (height > 0) lv_obj_set_height(label, height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int32_t width,
                             int32_t height, int32_t radius, uint32_t fill,
                             uint32_t border, const lv_font_t *font)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, radius, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = make_label(button, text, font, COLOR_TEXT, width - 16, height - 12);
    lv_obj_center(label);
    return button;
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
    esp_err_t err = ESP_OK;
    if (s_requested_mode == APP_MODE_PUSH_TO_TALK) {
        if (code == LV_EVENT_PRESSED) {
            err = push_to_talk_on_pressed();
            s_listening = (err == ESP_OK);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            err = push_to_talk_on_released();
            s_listening = false;
        }
    } else if (s_requested_mode == APP_MODE_MEDIA_REMOTE && code == LV_EVENT_CLICKED) {
        err = media_remote_send(MEDIA_COMMAND_PLAY_PAUSE);
    } else if (s_requested_mode == APP_MODE_HOME && code == LV_EVENT_CLICKED) {
        err = mode_router_switch_to(APP_MODE_MEDIA_REMOTE);
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE && err != ESP_ERR_NOT_SUPPORTED) {
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

static void add_dial(lv_obj_t *screen)
{
    lv_obj_t *bezel = lv_obj_create(screen);
    lv_obj_set_size(bezel, 438, 438);
    lv_obj_center(bezel);
    lv_obj_set_style_radius(bezel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(bezel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(bezel, lv_color_hex(0x272320), 0);
    lv_obj_set_style_border_width(bezel, 2, 0);
    lv_obj_clear_flag(bezel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inner = lv_obj_create(screen);
    lv_obj_set_size(inner, 402, 402);
    lv_obj_center(inner);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(inner, lv_color_hex(0x211C19), 0);
    lv_obj_set_style_border_width(inner, 1, 0);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < TICK_COUNT; ++i) {
        const float angle = ((float)i * 6.0f - 90.0f) * 0.01745329252f;
        const int length = (i % 5 == 0) ? 14 : 7;
        const int radius = 198;
        const int x = FACE_CENTER + (int)lroundf(cosf(angle) * radius);
        const int y = FACE_CENTER + (int)lroundf(sinf(angle) * radius);
        lv_obj_t *tick = lv_obj_create(screen);
        s_ticks[i] = tick;
        lv_obj_set_size(tick, (i % 5 == 0) ? 3 : 2, length);
        lv_obj_set_pos(tick, x - 1, y - length / 2);
        lv_obj_set_style_radius(tick, 1, 0);
        lv_obj_set_style_bg_color(tick, lv_color_hex((i % 5 == 0) ? 0x704532 : COLOR_TICK), 0);
        lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tick, 0, 0);
        lv_obj_set_style_transform_rotation(tick, (i * 60), 0);
        lv_obj_clear_flag(tick, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void refresh_ui(lv_timer_t *timer)
{
    (void)timer;
    if (s_powering_off) {
        set_label(s_title, "正在关机");
        set_label(s_subtitle, "圆屏遥控器");
        set_label(s_center_label, "再见");
        set_label(s_state, "按下下键开机");
        lv_obj_add_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_prev_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_next_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mode_controls, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const app_mode_t mode = s_requested_mode;
    const bool connected = ble_hid_is_connected();
    lv_obj_add_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_prev_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_next_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mode_controls, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_center_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_center_button, lv_color_hex(COLOR_BUTTON), 0);
    lv_obj_set_style_border_color(s_center_button, lv_color_hex(0x8B4A35), 0);
    for (int i = 0; i < TICK_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_ticks[i], lv_color_hex((i % 5 == 0) ? 0x704532 : COLOR_TICK), 0);
    }

    if (mode == APP_MODE_PUSH_TO_TALK) {
        set_label(s_title, "PTT");
        set_label(s_subtitle, "按住发送主机快捷键");
        set_label(s_center_label, s_listening ? "PTT\n键已按下" : "按住\n说话");
        set_label(s_state, s_listening ? "快捷键已发送 · 松开释放" :
                  !connected ? "蓝牙未连接" :
                  !ble_hid_ptt_key_configured() ? "PTT 键位未配置" : "等待按住");
        set_label(s_bottom_note, "此模式不采集麦克风音频");
        lv_obj_set_style_bg_color(s_center_button, lv_color_hex(s_listening ? COLOR_AMBER : 0x211713), 0);
        lv_obj_set_style_border_color(s_center_button, lv_color_hex(s_listening ? 0xFF9B73 : 0xA64E35), 0);
        if (s_listening) {
            for (int i = 0; i < TICK_COUNT; ++i) {
                if (i % 2 == 0) lv_obj_set_style_bg_color(s_ticks[i], lv_color_hex(COLOR_AMBER), 0);
            }
        }
    } else if (mode == APP_MODE_MEDIA_REMOTE) {
        set_label(s_title, "媒体遥控");
        set_label(s_subtitle, "蓝牙媒体按键");
        set_label(s_center_label, "播放\n暂停");
        set_label(s_state, connected ? "蓝牙已连接" : "蓝牙未连接");
        set_label(s_bottom_note, "曲名、进度与音量值不从主机读取");
        lv_obj_clear_flag(s_media_controls, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_prev_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_next_button, LV_OBJ_FLAG_HIDDEN);
    } else if (mode == APP_MODE_STANDBY_PET) {
        set_label(s_title, "待机");
        set_label(s_subtitle, "圆屏遥控器");
        set_label(s_center_label, "^_^\n待机");
        set_label(s_state, connected ? "蓝牙已连接" : "蓝牙未连接");
        set_label(s_bottom_note, "按下方按键返回主菜单");
    } else {
        set_label(s_title, "圆屏遥控器");
        set_label(s_subtitle, connected ? "蓝牙已连接" : "蓝牙未连接");
        set_label(s_center_label, "媒体\n遥控");
        set_label(s_state, "选择一个模式");
        set_label(s_bottom_note, "");
        lv_obj_clear_flag(s_mode_controls, LV_OBJ_FLAG_HIDDEN);
    }
}

esp_err_t board_port_init(void)
{
    lv_display_t *display = bsp_display_start();
    ESP_RETURN_ON_FALSE(display != NULL, ESP_FAIL, TAG, "Waveshare BSP display/touch init failed");
    ESP_RETURN_ON_ERROR(bsp_display_lock(-1), TAG, "LVGL lock failed");

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    add_dial(screen);

    s_title = make_label(screen, "圆屏遥控器", UI_FONT, COLOR_TEXT, 390, 34);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 56);
    s_subtitle = make_label(screen, "蓝牙媒体按键", UI_FONT, COLOR_MUTED, 390, 30);
    lv_obj_align(s_subtitle, LV_ALIGN_TOP_MID, 0, 91);

    s_center_button = make_button(screen, "", 210, 210, LV_RADIUS_CIRCLE,
                                  COLOR_BUTTON, 0x8B4A35, UI_FONT);
    lv_obj_align(s_center_button, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_border_width(s_center_button, 4, 0);
    lv_obj_set_style_shadow_color(s_center_button, lv_color_hex(0x73321F), 0);
    lv_obj_set_style_shadow_width(s_center_button, 24, 0);
    lv_obj_set_style_shadow_opa(s_center_button, LV_OPA_20, 0);
    lv_obj_add_event_cb(s_center_button, main_button_event, LV_EVENT_ALL, NULL);
    s_center_label = lv_obj_get_child(s_center_button, 0);
    lv_obj_set_style_text_font(s_center_label, UI_FONT, 0);

    s_state = make_label(screen, "蓝牙未连接", UI_FONT, COLOR_TEXT, 390, 30);
    lv_obj_align(s_state, LV_ALIGN_TOP_MID, 0, 126);
    s_bottom_note = make_label(screen, "曲名、进度与音量值不从主机读取", UI_FONT,
                               COLOR_MUTED, 414, 26);
    lv_obj_align(s_bottom_note, LV_ALIGN_BOTTOM_MID, 0, -39);

    /* Side transport controls remain physically separated from the center play/pause surface. */
    s_prev_button = make_button(screen, "上一首", 76, 62, LV_RADIUS_CIRCLE,
                                 COLOR_BUTTON, 0x45352E, UI_FONT);
    lv_obj_align(s_prev_button, LV_ALIGN_CENTER, -161, 5);
    lv_obj_add_event_cb(s_prev_button, media_control_event, LV_EVENT_CLICKED,
                        (void *)(intptr_t)MEDIA_COMMAND_PREVIOUS);
    s_next_button = make_button(screen, "下一首", 76, 62, LV_RADIUS_CIRCLE,
                                 COLOR_BUTTON, 0x45352E, UI_FONT);
    lv_obj_align(s_next_button, LV_ALIGN_CENTER, 161, 5);
    lv_obj_add_event_cb(s_next_button, media_control_event, LV_EVENT_CLICKED,
                        (void *)(intptr_t)MEDIA_COMMAND_NEXT);

    lv_obj_add_flag(s_prev_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_next_button, LV_OBJ_FLAG_HIDDEN);

    s_media_controls = lv_obj_create(screen);
    lv_obj_set_size(s_media_controls, 270, 48);
    lv_obj_align(s_media_controls, LV_ALIGN_BOTTOM_MID, 0, -75);
    lv_obj_set_style_bg_opa(s_media_controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_media_controls, 0, 0);
    lv_obj_set_style_pad_all(s_media_controls, 0, 0);
    lv_obj_set_style_pad_column(s_media_controls, 12, 0);
    lv_obj_set_flex_flow(s_media_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_media_controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    static const struct { const char *text; media_command_t command; } controls[] = {
        { "音量−", MEDIA_COMMAND_VOLUME_DOWN },
        { "静音", MEDIA_COMMAND_MUTE },
        { "音量+", MEDIA_COMMAND_VOLUME_UP },
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        lv_obj_t *button = make_button(s_media_controls, controls[i].text, 78, 44, 14,
                                       COLOR_BUTTON, 0x45352E, UI_FONT);
        lv_obj_add_event_cb(button, media_control_event, LV_EVENT_CLICKED,
                            (void *)(intptr_t)controls[i].command);
    }

    s_mode_controls = lv_obj_create(screen);
    lv_obj_set_size(s_mode_controls, 300, 58);
    lv_obj_align(s_mode_controls, LV_ALIGN_BOTTOM_MID, 0, -70);
    lv_obj_set_style_bg_opa(s_mode_controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mode_controls, 0, 0);
    lv_obj_set_style_pad_all(s_mode_controls, 0, 0);
    lv_obj_set_style_pad_column(s_mode_controls, 8, 0);
    lv_obj_set_flex_flow(s_mode_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_mode_controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    static const struct { const char *text; app_mode_t mode; } modes[] = {
        { "媒体", APP_MODE_MEDIA_REMOTE }, { "PTT", APP_MODE_PUSH_TO_TALK },
        { "待机", APP_MODE_STANDBY_PET },
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        lv_obj_t *button = make_button(s_mode_controls, modes[i].text, 90, 54, 15,
                                       COLOR_BUTTON, 0x45352E, UI_FONT);
        lv_obj_add_event_cb(button, mode_button_event, LV_EVENT_CLICKED,
                            (void *)(intptr_t)modes[i].mode);
    }

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

void board_port_ui_lock(void)
{
    (void)bsp_display_lock(-1);
}

void board_port_ui_unlock(void)
{
    bsp_display_unlock();
}

void board_port_show_power_off(void)
{
    s_powering_off = true;
}
