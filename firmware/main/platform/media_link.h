#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "media_remote.h"

/* Media transport. Today: BLE HID Consumer Control (shared device in ble_hid.c). */
esp_err_t media_link_start(void);
void media_link_stop(void);
esp_err_t media_link_send_command(media_command_t command);
bool media_link_is_connected(void);
