#pragma once

#include "esp_err.h"
#include "pet_commands.h"

esp_err_t pet_standby_enter(void);
void pet_standby_exit(void);
void pet_standby_on_command(uint16_t command_id);
