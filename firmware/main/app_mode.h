#pragma once

typedef enum {
    APP_MODE_STANDBY_PET = 0,
    APP_MODE_MEDIA_REMOTE,
    APP_MODE_PUSH_TO_TALK,
    APP_MODE_HOME,          /* main menu; owns no module lifecycle */
    APP_MODE_BADGE,         /* 电子吧唧: looping clip + Wi-Fi upload page */
    APP_MODE_BADGE_TOOLS,   /* RAM-only stopwatch and bubble-wrap toy */
} app_mode_t;

const char *app_mode_name(app_mode_t mode);
