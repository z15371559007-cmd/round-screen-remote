#include "hw_buttons.h"

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pmu_axp2101.h"
#include "sdkconfig.h"

#define BOOT_GPIO        GPIO_NUM_0
#define POLL_MS          10
#define DEBOUNCE_TICKS   3
#define PWR_POLL_EVERY   2   /* PMU edges every 20 ms */

static const char *TAG = "hw_buttons";
static hw_button_cb_t s_cb;
static bool s_started;

static void emit(hw_button_event_t event)
{
    if (s_cb != NULL) s_cb(event);
}

static void button_task(void *arg)
{
    (void)arg;
    bool boot_stable = false;
    int boot_count = 0;
    int64_t boot_down_us = 0;
    bool boot_long_fired = false;
    bool pwr_down = false;
    unsigned tick = 0;
    const int64_t long_us = (int64_t)CONFIG_ROUND_REMOTE_BOOT_LONG_PRESS_MS * 1000;

    for (;;) {
        /* BOOT: active low with pull-up, debounced. */
        bool raw = gpio_get_level(BOOT_GPIO) == 0;
        if (raw != boot_stable) {
            if (++boot_count >= DEBOUNCE_TICKS) {
                boot_stable = raw;
                boot_count = 0;
                if (boot_stable) {
                    boot_down_us = esp_timer_get_time();
                    boot_long_fired = false;
                } else if (!boot_long_fired) {
                    emit(HW_BUTTON_BOOT_SHORT);
                }
            }
        } else {
            boot_count = 0;
        }
        if (boot_stable && !boot_long_fired && esp_timer_get_time() - boot_down_us >= long_us) {
            boot_long_fired = true;
            emit(HW_BUTTON_BOOT_LONG);
        }

        /* PWR: edges latched by the AXP2101. */
        if (++tick % PWR_POLL_EVERY == 0 && pmu_is_ready()) {
            bool pressed = false, released = false;
            if (pmu_poll_pwr_edges(&pressed, &released) == ESP_OK) {
                if (pressed && !pwr_down) {
                    pwr_down = true;
                    emit(HW_BUTTON_PWR_DOWN);
                }
                if (released && pwr_down) {
                    pwr_down = false;
                    emit(HW_BUTTON_PWR_UP);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

esp_err_t hw_buttons_start(hw_button_cb_t cb)
{
    if (s_started) return ESP_OK;
    s_cb = cb;
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "BOOT gpio");
    BaseType_t ok = xTaskCreate(button_task, "hw_buttons", 3072, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create");
    s_started = true;
    ESP_LOGI(TAG, "Side keys active: PWR(upper)=PTT/play-pause, BOOT(lower)=home/long-press power-off");
    return ESP_OK;
}
