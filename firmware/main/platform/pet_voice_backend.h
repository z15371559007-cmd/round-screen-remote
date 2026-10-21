#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef void (*pet_voice_command_cb_t)(uint16_t command_id);

esp_err_t pet_voice_backend_start(pet_voice_command_cb_t callback);
void pet_voice_backend_stop(void);
