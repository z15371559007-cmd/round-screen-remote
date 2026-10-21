#include "esp_log.h"
#include "esp_err.h"
#include "pet_voice_backend.h"

static const char *TAG = "pet_voice";

esp_err_t pet_voice_backend_start(pet_voice_command_cb_t callback)
{
    (void)callback;
    ESP_LOGW(TAG, "ESP-SR AFE/WakeNet/MultiNet backend not connected");
    return ESP_ERR_NOT_SUPPORTED;
}

void pet_voice_backend_stop(void)
{
}
