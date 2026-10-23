#pragma once

#include <stdbool.h>
#include "esp_err.h"

/*
 * Push-to-talk = HOLD a BLE HID keyboard key (default F13, usage 0x68, no modifiers).
 * Press sends one key-down report; the key stays held on the host until release sends the
 * empty report. Several sources (touch, side key) may hold at once; the key goes up only
 * when the last source releases. The key is force-released on: mode exit, BLE disconnect,
 * and CONFIG_ROUND_REMOTE_PTT_MAX_HOLD_MS (stuck-key watchdog, 0 = off).
 */
typedef enum {
    PTT_SOURCE_TOUCH = 1u << 0,   /* on-screen PTT button (UI) */
    PTT_SOURCE_KEY   = 1u << 1,   /* upper side key (PWR) on the PTT page */
} ptt_source_t;

/* Called when the effective held state changes (true = key down on host, false = released,
 * including forced releases). Runs on the caller / BLE / esp_timer task: take
 * board_port_ui_lock() yourself before touching LVGL. */
typedef void (*ptt_state_cb_t)(bool held, void *ctx);

esp_err_t push_to_talk_enter(void);
void push_to_talk_exit(void);

/* Source-aware hold API (used by button_actions with PTT_SOURCE_KEY). */
esp_err_t push_to_talk_press(ptt_source_t source);
esp_err_t push_to_talk_release(ptt_source_t source);
/* Releases every source and sends key-up. Safe to call any time. */
void push_to_talk_release_all(void);

/* ---- UI API (touch button) ---- */
esp_err_t ptt_press(void);     /* = push_to_talk_press(PTT_SOURCE_TOUCH) */
esp_err_t ptt_release(void);   /* = push_to_talk_release(PTT_SOURCE_TOUCH) */
bool ptt_is_held(void);        /* key currently held down on the host */
bool ptt_is_ready(void);       /* PTT page active + BLE connected + usage configured */
void ptt_set_state_cb(ptt_state_cb_t cb, void *ctx);

/* Backwards-compatible names used by the current UI; same as ptt_press()/ptt_release(). */
esp_err_t push_to_talk_on_pressed(void);
esp_err_t push_to_talk_on_released(void);
