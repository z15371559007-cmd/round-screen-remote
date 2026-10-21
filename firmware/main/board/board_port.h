#pragma once

#include "esp_err.h"
#include "app_mode.h"
#include "pet_commands.h"

esp_err_t board_port_init(void);
void board_port_render_mode(app_mode_t mode);
void board_port_show_pet_expression(pet_expression_t expression);
void board_port_set_listening(bool listening);
