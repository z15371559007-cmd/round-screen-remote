#pragma once

#include "esp_err.h"

esp_err_t push_to_talk_enter(void);
void push_to_talk_exit(void);
esp_err_t push_to_talk_on_pressed(void);
esp_err_t push_to_talk_on_released(void);
