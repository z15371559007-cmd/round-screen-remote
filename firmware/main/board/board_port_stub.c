#include "esp_log.h"
#include "board_port.h"

static const char *TAG = "board_port";

esp_err_t board_port_init(void)
{
    ESP_LOGW(TAG, "Display/touch driver not connected; using scaffold adapter");
    return ESP_OK;
}

void board_port_render_mode(app_mode_t mode)
{
    ESP_LOGI(TAG, "UI mode requested: %s", app_mode_name(mode));
}

void board_port_show_pet_expression(pet_expression_t expression)
{
    ESP_LOGI(TAG, "Pet expression requested: %d", (int)expression);
}

void board_port_set_listening(bool listening)
{
    ESP_LOGI(TAG, "Pet listening indicator: %s", listening ? "on" : "off");
}

void board_port_ui_lock(void) {}
void board_port_ui_unlock(void) {}

void board_port_show_power_off(void)
{
    ESP_LOGI(TAG, "Power-off requested");
}

void board_port_set_ptt_failed(bool failed)
{
    (void)failed;
}
