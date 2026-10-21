#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "pet_standby.h"
#include "board_port.h"
#include "pet_voice_backend.h"

static const char *TAG = "pet_standby";
static bool s_active;

esp_err_t pet_standby_enter(void)
{
    s_active = true;
    board_port_show_pet_expression(PET_EXPRESSION_IDLE);
    esp_err_t err = pet_voice_backend_start(pet_standby_on_command);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Voice recognition is a placeholder; pet remains idle (%s)", esp_err_to_name(err));
    }
    return ESP_OK;
}

void pet_standby_exit(void)
{
    if (s_active) {
        pet_voice_backend_stop();
        board_port_set_listening(false);
        s_active = false;
    }
}

void pet_standby_on_command(uint16_t command_id)
{
    if (!s_active) {
        return;
    }
    bool known = false;
    pet_expression_t expression = pet_command_expression(command_id, &known);
    if (!known) {
        ESP_LOGW(TAG, "Ignoring unmapped pet command id %u", (unsigned)command_id);
        return;
    }
    board_port_show_pet_expression(expression);
}
