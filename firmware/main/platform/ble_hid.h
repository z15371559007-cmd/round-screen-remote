#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    BLE_HID_MEDIA_PLAY_PAUSE = 0,
    BLE_HID_MEDIA_PREVIOUS,
    BLE_HID_MEDIA_NEXT,
    BLE_HID_MEDIA_VOLUME_UP,
    BLE_HID_MEDIA_VOLUME_DOWN,
    BLE_HID_MEDIA_MUTE,
    BLE_HID_MEDIA_STOP,
} ble_hid_media_command_t;

/* Called from the HID event task on connect (true) / disconnect (false). Keep it short;
 * take board_port_ui_lock() yourself before touching LVGL. */
typedef void (*ble_hid_conn_cb_t)(bool connected, void *ctx);

/* Initializes one shared BLE HID keyboard/consumer-control peripheral. Idempotent. */
esp_err_t ble_hid_start(void);
bool ble_hid_is_connected(void);

/* Radio status for the UI: stack never started / failed to start / advertising (discoverable) / connected. */
typedef enum { BLE_HID_STATE_OFF = 0, BLE_HID_STATE_FAILED, BLE_HID_STATE_ADVERTISING, BLE_HID_STATE_CONNECTED } ble_hid_state_t;
ble_hid_state_t ble_hid_state(void);
/* Advertised name, shown on screen so the user knows what to pick in the host's Bluetooth list. */
const char *ble_hid_device_name(void);

/* Up to 4 listeners. Same (cb, ctx) pair is registered only once. */
esp_err_t ble_hid_register_conn_cb(ble_hid_conn_cb_t cb, void *ctx);

/* True when CONFIG_ROUND_REMOTE_PTT_HID_USAGE is a sendable key (0x04-0xDF). Default 0x68 = F13. */
bool ble_hid_ptt_key_configured(void);
uint8_t ble_hid_ptt_usage(void);

/* PTT keyboard report: pressed=true holds the PTT key down (one report), false sends the empty
 * report (key up). Releasing while disconnected / already released is a no-op returning ESP_OK.
 * Returns ESP_ERR_INVALID_STATE when pressing without a host, ESP_ERR_NOT_SUPPORTED if usage is 0. */
esp_err_t ble_hid_set_pressed(bool pressed);
bool ble_hid_ptt_is_down(void);
/* One-shot keyboard tap. Refuses to overwrite a held PTT key; usage is a standard key usage. */
esp_err_t ble_hid_send_key(uint8_t modifiers, uint8_t usage);

/* Emits a standard Bluetooth HID Consumer Control tap (press + ~25 ms + release). */
esp_err_t ble_hid_send_media(ble_hid_media_command_t command);

/* Sends empty keyboard + consumer reports (best effort) so nothing can stay stuck on the host. */
void ble_hid_release_all(void);
