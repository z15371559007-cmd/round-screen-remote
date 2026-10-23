#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* "电子吧唧" mode: loops a short clip stored in the `badge` flash partition and serves an
 * upload page over a Wi-Fi hotspot. Clip format (little endian), written by the web page:
 *   "ORBV" u16 version=1, u16 fps, u16 width=466, u16 height=466, u32 frame_count
 *   then frame_count x { u32 jpeg_len, jpeg bytes, zero pad to 4 }.
 * enter/exit never block (they may be called with the LVGL lock held). */
esp_err_t badge_enter(void);
void badge_exit(void);

#define BADGE_WIFI_SSID "Omarchy-Orb"
#define BADGE_WIFI_PASS "omarchy1"
#define BADGE_URL       "192.168.4.1"

typedef enum {
    BADGE_STATE_NO_VIDEO = 0,
    BADGE_STATE_PLAYING,
    BADGE_STATE_RECEIVING,
    BADGE_STATE_STARTING,
    BADGE_STATE_ERROR,
} badge_state_t;

badge_state_t badge_state(void);
/* Upload progress 0-100 while receiving. */
int badge_progress(void);
