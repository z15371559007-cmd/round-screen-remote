#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    BLE_HID_MEDIA_PLAY_PAUSE = 0,
    BLE_HID_MEDIA_PREVIOUS,
    BLE_HID_MEDIA_NEXT,
    BLE_HID_MEDIA_VOLUME_UP,
    BLE_HID_MEDIA_VOLUME_DOWN,
    BLE_HID_MEDIA_MUTE,
} ble_hid_media_command_t;

/* Initializes one shared BLE HID keyboard/consumer-control peripheral. */
esp_err_t ble_hid_start(void);
bool ble_hid_is_connected(void);
bool ble_hid_ptt_key_configured(void);

/* PTT keyboard report. Returns ESP_ERR_NOT_SUPPORTED until a HID usage is selected. */
esp_err_t ble_hid_set_pressed(bool pressed);

/* Emits a standard Bluetooth HID Consumer Control tap. */
esp_err_t ble_hid_send_media(ble_hid_media_command_t command);
