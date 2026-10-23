#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "push_to_talk.h"
#include "ble_hid.h"

static const char *TAG = "push_to_talk";
static bool s_active;
static uint32_t s_sources;              /* bitmask of ptt_source_t currently holding */
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buf;
static esp_timer_handle_t s_watchdog;
static ptt_state_cb_t s_state_cb;
static void *s_state_ctx;

static void lock(void)   { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void notify(bool held)
{
    ptt_state_cb_t cb = s_state_cb;
    if (cb) cb(held, s_state_ctx);
}

static void watchdog_stop(void)
{
    if (s_watchdog) (void)esp_timer_stop(s_watchdog);
}

static void watchdog_start(void)
{
#if CONFIG_ROUND_REMOTE_PTT_MAX_HOLD_MS > 0
    if (s_watchdog) {
        (void)esp_timer_stop(s_watchdog);
        (void)esp_timer_start_once(s_watchdog, (uint64_t)CONFIG_ROUND_REMOTE_PTT_MAX_HOLD_MS * 1000ULL);
    }
#endif
}

/* Caller holds the lock. Returns true if the key was down. */
static bool force_release_locked(void)
{
    const bool was_held = s_sources != 0 || ble_hid_ptt_is_down();
    s_sources = 0;
    watchdog_stop();
    (void)ble_hid_set_pressed(false);
    return was_held;
}

static void watchdog_fired(void *arg)
{
    (void)arg;
    lock();
    bool was = force_release_locked();
    unlock();
    if (was) {
        ESP_LOGW(TAG, "PTT held longer than %d ms; forced key-up", CONFIG_ROUND_REMOTE_PTT_MAX_HOLD_MS);
        notify(false);
    }
}

static void on_ble_conn(bool connected, void *ctx)
{
    (void)ctx;
    if (connected) return;
    lock();
    bool was = s_sources != 0;
    s_sources = 0;          /* host already dropped the key with the link */
    watchdog_stop();
    unlock();
    if (was) {
        ESP_LOGW(TAG, "BLE disconnected while PTT held; released");
        notify(false);
    }
}

static void init_once(void)
{
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
    const esp_timer_create_args_t args = { .callback = watchdog_fired, .name = "ptt_wd" };
    if (esp_timer_create(&args, &s_watchdog) != ESP_OK) s_watchdog = NULL;
    (void)ble_hid_register_conn_cb(on_ble_conn, NULL);
}

esp_err_t push_to_talk_enter(void)
{
    init_once();
    esp_err_t err = ble_hid_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BLE HID adapter is not connected yet (%s)", esp_err_to_name(err));
    }
    lock();
    s_active = true;
    unlock();
    if (!ble_hid_ptt_key_configured()) {
        ESP_LOGW(TAG, "PTT HID usage 0x%02X not sendable; PTT disabled", ble_hid_ptt_usage());
    }
    return ESP_OK;
}

void push_to_talk_exit(void)
{
    if (!s_lock) return;
    lock();
    bool was = force_release_locked();   /* no stuck key when the page changes mid-hold */
    s_active = false;
    unlock();
    if (was) notify(false);
}

void push_to_talk_release_all(void)
{
    if (!s_lock) return;
    lock();
    bool was = force_release_locked();
    unlock();
    if (was) notify(false);
}

esp_err_t push_to_talk_press(ptt_source_t source)
{
    if (!s_lock) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_OK;
    bool became_held = false;
    lock();
    if (!s_active) {
        err = ESP_ERR_INVALID_STATE;
    } else if (s_sources & source) {
        err = ESP_OK;                     /* repeated press from the same source */
    } else if (s_sources != 0) {
        s_sources |= source;              /* key already down for another source */
    } else {
        err = ble_hid_set_pressed(true);
        if (err == ESP_OK) {
            s_sources = source;
            became_held = true;
            watchdog_start();
        }
    }
    unlock();
    if (became_held) notify(true);
    return err;
}

esp_err_t push_to_talk_release(ptt_source_t source)
{
    if (!s_lock) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_OK;
    bool released = false;
    lock();
    if (s_sources & source) {
        s_sources &= ~(uint32_t)source;
        if (s_sources == 0) {
            watchdog_stop();
            err = ble_hid_set_pressed(false);
            released = true;
        }
    } else if (!s_active) {
        err = ESP_ERR_INVALID_STATE;
    }
    unlock();
    if (released) notify(false);
    return err;
}

esp_err_t ptt_press(void)   { return push_to_talk_press(PTT_SOURCE_TOUCH); }
esp_err_t ptt_release(void) { return push_to_talk_release(PTT_SOURCE_TOUCH); }
esp_err_t push_to_talk_on_pressed(void)  { return ptt_press(); }
esp_err_t push_to_talk_on_released(void) { return ptt_release(); }

bool ptt_is_held(void)
{
    return ble_hid_ptt_is_down();
}

bool ptt_is_ready(void)
{
    return s_active && ble_hid_is_connected() && ble_hid_ptt_key_configured();
}

void ptt_set_state_cb(ptt_state_cb_t cb, void *ctx)
{
    s_state_ctx = ctx;
    s_state_cb = cb;
}
