#pragma once

#include <stdbool.h>
#include "esp_err.h"

/*
 * Minimal AXP2101 power-management access for the side PWR key and soft power-off.
 * The PWR key is wired to the AXP2101 PWRON pin, not to an ESP32 GPIO, so its
 * press/release is read from the PMU's PWRON edge interrupt flags over I2C.
 */
esp_err_t pmu_init(void);
bool pmu_is_ready(void);

/* Reads and clears pending PWR edges. Either flag may be set; both means a tap shorter than one poll. */
esp_err_t pmu_poll_pwr_edges(bool *pressed, bool *released);

/* Cuts the PMU output rails. Does not return on success. */
esp_err_t pmu_power_off(void);
