#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "app_mode.h"
#include "pet_commands.h"

esp_err_t board_port_init(void);
void board_port_render_mode(app_mode_t mode);
void board_port_show_pet_expression(pet_expression_t expression);
void board_port_set_listening(bool listening);
/* PTT was attempted (touch or key) but could not be sent: show it on screen. */
void board_port_set_ptt_failed(bool failed);

/* Serialises non-LVGL callers (e.g. physical keys) with the UI thread. */
void board_port_ui_lock(void);
void board_port_ui_unlock(void);
void board_port_show_power_off(void);
/* Badge player: swap in a decoded 466x466 RGB565 frame (takes the UI lock itself). */
void board_port_badge_show_frame(const void *image_dsc);
