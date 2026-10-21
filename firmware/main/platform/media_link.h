#pragma once

#include "esp_err.h"
#include "media_remote.h"

esp_err_t media_link_start(void);
void media_link_stop(void);
esp_err_t media_link_send_command(media_command_t command);
