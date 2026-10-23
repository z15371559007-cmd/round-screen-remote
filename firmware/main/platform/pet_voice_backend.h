#pragma once

/*
 * Device-side voice backend for the standby pet (on-board dual mic -> wake word -> command id).
 *
 * STATUS: STUB (CONFIG_ROUND_REMOTE_PET_VOICE_BACKEND_STUB). pet_voice_backend_start() logs and
 * returns ESP_ERR_NOT_SUPPORTED; the pet stays idle. A real backend (planned: ESP-SR
 * AFE + WakeNet + MultiNet on the board's mic/codec) must implement exactly this header in its
 * own file (e.g. pet_voice_backend_esp_sr.c), selected by CONFIG_ROUND_REMOTE_PET_VOICE_BACKEND_ESP_SR.
 *
 * Contract for a real backend:
 *   - start(): open mic, start recognition task, return ESP_OK. Idempotent.
 *   - on each recognised phrase call callback(command_id) from the recognition task, where
 *     command_id is a pet_command_id_t (pet_commands.h: 1 greeting, 2 happy, 3 sleep).
 *     Unknown ids are ignored by pet_standby.
 *   - stop(): stop recognition and release the mic. Safe to call when not started.
 *   - Never call LVGL directly; pet_standby / board_port do the drawing.
 * PTT does not use this backend: PTT holds a HID key and the computer records the audio.
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef void (*pet_voice_command_cb_t)(uint16_t command_id);

/* ESP_OK when listening; ESP_ERR_NOT_SUPPORTED from the stub. */
esp_err_t pet_voice_backend_start(pet_voice_command_cb_t callback);
void pet_voice_backend_stop(void);
/* false for the stub; UI may show "语音未接入" instead of a listening indicator. */
bool pet_voice_backend_available(void);
/* true while the backend is actually listening. */
bool pet_voice_backend_is_listening(void);
