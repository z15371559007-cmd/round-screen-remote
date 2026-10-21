#pragma once

typedef enum {
    APP_MODE_STANDBY_PET = 0,
    APP_MODE_MEDIA_REMOTE,
    APP_MODE_PUSH_TO_TALK,
} app_mode_t;

const char *app_mode_name(app_mode_t mode);
