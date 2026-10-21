#pragma once

#include "esp_err.h"
#include "app_mode.h"

esp_err_t mode_router_init(void);
esp_err_t mode_router_switch_to(app_mode_t mode);
app_mode_t mode_router_current(void);
