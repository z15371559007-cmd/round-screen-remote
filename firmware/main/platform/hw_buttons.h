#pragma once

#include "esp_err.h"

/* Physical side keys. Upper key = PWR (via AXP2101), lower key = BOOT (GPIO0). */
typedef enum {
    HW_BUTTON_PWR_DOWN = 0,   /* upper key pressed */
    HW_BUTTON_PWR_UP,         /* upper key released */
    HW_BUTTON_BOOT_SHORT,     /* lower key tapped */
    HW_BUTTON_BOOT_LONG,      /* lower key held past the long-press threshold (fires once, while held) */
} hw_button_event_t;

typedef void (*hw_button_cb_t)(hw_button_event_t event);

/* Starts one polling task that reports raw key events. It knows nothing about modes or actions. */
esp_err_t hw_buttons_start(hw_button_cb_t cb);
