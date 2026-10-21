#pragma once

#include "esp_err.h"

typedef enum {
    MEDIA_COMMAND_PLAY_PAUSE = 0,
    MEDIA_COMMAND_PREVIOUS,
    MEDIA_COMMAND_NEXT,
    MEDIA_COMMAND_STOP,
} media_command_t;

esp_err_t media_remote_enter(void);
void media_remote_exit(void);
esp_err_t media_remote_send(media_command_t command);
