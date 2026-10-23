#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    MEDIA_COMMAND_PLAY_PAUSE = 0,
    MEDIA_COMMAND_PREVIOUS,
    MEDIA_COMMAND_NEXT,
    MEDIA_COMMAND_STOP,
    MEDIA_COMMAND_VOLUME_UP,
    MEDIA_COMMAND_VOLUME_DOWN,
    MEDIA_COMMAND_MUTE,
} media_command_t;

/* Page lifecycle (mode_router only). */
esp_err_t media_remote_enter(void);
void media_remote_exit(void);

/* Sends a command only while the media page is active (ESP_ERR_INVALID_STATE otherwise). */
esp_err_t media_remote_send(media_command_t command);

/*
 * ---- UI / key API ----
 * Each call is one BLE HID Consumer Control tap (key down ~25 ms, key up). They work from any
 * page (the shared HID link is always up) and return ESP_ERR_INVALID_STATE when no host is
 * connected. Blocking ~25 ms; fine to call from an LVGL event callback.
 */
esp_err_t media_remote_play_pause(void);
esp_err_t media_remote_previous(void);
esp_err_t media_remote_next(void);
esp_err_t media_remote_volume_up(void);
esp_err_t media_remote_volume_down(void);
esp_err_t media_remote_mute(void);
esp_err_t media_remote_stop(void);
/* Any media_command_t, regardless of the active page. */
esp_err_t media_remote_command(media_command_t command);
bool media_remote_is_connected(void);
