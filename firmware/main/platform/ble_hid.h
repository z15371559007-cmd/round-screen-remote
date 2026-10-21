#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t ble_hid_start(void);
void ble_hid_stop(void);
esp_err_t ble_hid_set_pressed(bool pressed);
