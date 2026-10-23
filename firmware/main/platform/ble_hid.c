#include <stdbool.h>
#include <stdint.h>

#include "esp_bt.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_hid_common.h"
#include "esp_hid_gap.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "ble_hid.h"

static const char *TAG = "ble_hid";
static bool s_started;
static volatile bool s_failed;
static volatile bool s_advertising;
static esp_hidd_dev_t *s_hid_dev;
static volatile bool s_ptt_pressed;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buf;

#define CONN_CB_MAX 4
static struct { ble_hid_conn_cb_t cb; void *ctx; } s_conn_cbs[CONN_CB_MAX];

/* Keyboard (report 1) and Consumer Control (report 3) share one BLE HID device. */
static const uint8_t s_report_map[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x01,       /* Report ID (1) */
    0x05, 0x07,       /* Usage Page (Keyboard/Keypad) */
    0x19, 0xE0, 0x29, 0xE7, /* Modifier usages */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x81, 0x02,       /* 8 modifier bits */
    0x75, 0x08, 0x95, 0x01, 0x81, 0x01, /* Reserved byte */
    /* Six-key array covering usages 0x00-0xFF, so F13-F24 (0x68-0x73) are transmitted.
     * Logical Maximum uses the 2-byte form (0x26 0xFF 0x00): 0x25 0xFF would mean -1. */
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x07,
    0x19, 0x00, 0x2A, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06,
    0x81, 0x00,       /* Six-key array */
    0xC0,
    0x05, 0x0C,       /* Usage Page (Consumer) */
    0x09, 0x01,       /* Usage (Consumer Control) */
    0xA1, 0x01,
    0x85, 0x03,       /* Report ID (3) */
    0x15, 0x00, 0x26, 0xFF, 0x03,
    0x19, 0x00, 0x2A, 0xFF, 0x03,
    0x75, 0x10, 0x95, 0x01,
    0x81, 0x00,       /* 16-bit Consumer usage array */
    0xC0,
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    { .data = s_report_map, .len = sizeof(s_report_map) },
};

static esp_hid_device_config_t s_device_config = {
    .vendor_id = 0x303A,
    .product_id = 0x1001,
    .version = 0x0101, /* bumped when the report map changed (F13 support) */
    .device_name = "Omarchy Orb",
    .manufacturer_name = "Personal Project",
    .serial_number = "RSR-01",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static void lock(void)   { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { if (s_lock) xSemaphoreGive(s_lock); }

static void notify_conn(bool connected)
{
    for (int i = 0; i < CONN_CB_MAX; i++) {
        if (s_conn_cbs[i].cb) s_conn_cbs[i].cb(connected, s_conn_cbs[i].ctx);
    }
}

esp_err_t ble_hid_register_conn_cb(ble_hid_conn_cb_t cb, void *ctx)
{
    if (!cb) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < CONN_CB_MAX; i++) {
        if (s_conn_cbs[i].cb == cb && s_conn_cbs[i].ctx == ctx) return ESP_OK;
    }
    for (int i = 0; i < CONN_CB_MAX; i++) {
        if (!s_conn_cbs[i].cb) {
            s_conn_cbs[i].ctx = ctx;
            s_conn_cbs[i].cb = cb;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

static void hid_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;
    (void)base;
    esp_hidd_event_data_t *event = (esp_hidd_event_data_t *)event_data;
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT: {
        esp_err_t err = esp_hid_ble_gap_adv_start();
        if (err != ESP_OK) ESP_LOGE(TAG, "BLE advertising start failed: %s", esp_err_to_name(err));
        else { s_advertising = true; ESP_LOGI(TAG, "BLE HID advertising as \"%s\" (discoverable)", s_device_config.device_name); }
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
        /* A fresh link never inherits a held PTT key. */
        s_ptt_pressed = false;
        s_advertising = false;
        ESP_LOGI(TAG, "BLE HID host connected");
        notify_conn(true);
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        /* Hosts drop all keys of a vanished HID device; we just forget our state. */
        s_ptt_pressed = false;
        ESP_LOGI(TAG, "BLE HID host disconnected (reason %d)", event->disconnect.reason);
        notify_conn(false);
        if (esp_hid_ble_gap_adv_start() != ESP_OK) ESP_LOGW(TAG, "BLE advertising restart failed");
        else { s_advertising = true; ESP_LOGI(TAG, "BLE HID advertising again"); }
        break;
    default:
        break;
    }
}

esp_err_t ble_hid_start(void)
{
    if (s_started) return ESP_OK;
    if (!s_lock) s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Leftover NVS from the factory/demo firmware (merge_bin does not wipe it). It holds
         * nothing of ours except BLE bonds, so reformat instead of silently running without BLE. */
        ESP_LOGW(TAG, "NVS unusable (%s), probably left by previous firmware; erasing NVS partition", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s); BLE disabled", esp_err_to_name(err));
        s_failed = true;
        return err;
    }

    err = esp_hid_gap_init(HID_DEV_MODE);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_hid_gap_init failed: %s", esp_err_to_name(err)); s_failed = true; return err; }

    err = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_device_config.device_name);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_hid_ble_gap_adv_init failed: %s", esp_err_to_name(err)); s_failed = true; return err; }

    err = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %s", esp_err_to_name(err)); s_failed = true; return err; }

    err = esp_hidd_dev_init(&s_device_config, ESP_HID_TRANSPORT_BLE, hid_event_handler, &s_hid_dev);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_hidd_dev_init failed: %s", esp_err_to_name(err)); s_failed = true; return err; }

    s_started = true;
    ESP_LOGI(TAG, "BLE HID ready; PTT HID usage=0x%02X modifiers=0x%02X (0 means unconfigured)",
             CONFIG_ROUND_REMOTE_PTT_HID_USAGE, CONFIG_ROUND_REMOTE_PTT_HID_MODIFIERS);
    return ESP_OK;
}

ble_hid_state_t ble_hid_state(void)
{
    if (ble_hid_is_connected()) return BLE_HID_STATE_CONNECTED;
    if (s_failed) return BLE_HID_STATE_FAILED;
    if (s_advertising) return BLE_HID_STATE_ADVERTISING;
    return BLE_HID_STATE_OFF;
}

const char *ble_hid_device_name(void) { return s_device_config.device_name; }

bool ble_hid_is_connected(void)
{
    return s_hid_dev != NULL && esp_hidd_dev_connected(s_hid_dev);
}

bool ble_hid_ptt_key_configured(void)
{
    const int usage = CONFIG_ROUND_REMOTE_PTT_HID_USAGE;
    /* 0x00-0x03 are reserved; 0xE0-0xE7 are modifiers (use the modifier mask instead). */
    return usage >= 0x04 && usage <= 0xDF;
}

uint8_t ble_hid_ptt_usage(void)
{
    return (uint8_t)CONFIG_ROUND_REMOTE_PTT_HID_USAGE;
}

bool ble_hid_ptt_is_down(void)
{
    return s_ptt_pressed;
}

/* Caller holds the lock. */
static esp_err_t send_keyboard_report(uint8_t modifiers, uint8_t usage)
{
    if (!ble_hid_is_connected()) return ESP_ERR_INVALID_STATE;
    uint8_t report[8] = { modifiers, 0, usage, 0, 0, 0, 0, 0 };
    return esp_hidd_dev_input_set(s_hid_dev, 0, 1, report, sizeof(report));
}

esp_err_t ble_hid_set_pressed(bool pressed)
{
    esp_err_t err = ESP_OK;
    lock();
    if (!pressed) {
        /* Key up: always safe. If the link is gone the host already released the key. */
        if (s_ptt_pressed) {
            err = send_keyboard_report(0, 0);
            s_ptt_pressed = false;          /* never keep local "down" state after a release */
            if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;
        }
        unlock();
        return err;
    }

    if (!ble_hid_ptt_key_configured()) err = ESP_ERR_NOT_SUPPORTED;
    else if (!ble_hid_is_connected()) err = ESP_ERR_INVALID_STATE;
    else if (!s_ptt_pressed) {
        /* One key-down report; the host keeps the key held until the empty report arrives. */
        err = send_keyboard_report((uint8_t)CONFIG_ROUND_REMOTE_PTT_HID_MODIFIERS,
                                   (uint8_t)CONFIG_ROUND_REMOTE_PTT_HID_USAGE);
        if (err == ESP_OK) s_ptt_pressed = true;
    }
    unlock();
    return err;
}

esp_err_t ble_hid_send_key(uint8_t modifiers, uint8_t usage)
{
    if (usage < 0x04 || usage > 0xDF) return ESP_ERR_INVALID_ARG;
    lock();
    if (!ble_hid_is_connected()) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }
    /* A one-shot key must never replace the currently held push-to-talk key report. */
    if (s_ptt_pressed) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = send_keyboard_report(modifiers, usage);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(25));
        err = send_keyboard_report(0, 0);
        if (err != ESP_OK && ble_hid_is_connected()) {
            /* Best-effort second key-up if the first release notification was rejected. */
            (void)send_keyboard_report(0, 0);
        }
    }
    unlock();
    return err;
}

static uint16_t consumer_usage(ble_hid_media_command_t command)
{
    switch (command) {
    case BLE_HID_MEDIA_PLAY_PAUSE: return 0x00CD; /* Play/Pause */
    case BLE_HID_MEDIA_PREVIOUS:   return 0x00B6; /* Scan Previous Track */
    case BLE_HID_MEDIA_NEXT:       return 0x00B5; /* Scan Next Track */
    case BLE_HID_MEDIA_VOLUME_UP:  return 0x00E9; /* Volume Increment */
    case BLE_HID_MEDIA_VOLUME_DOWN:return 0x00EA; /* Volume Decrement */
    case BLE_HID_MEDIA_MUTE:       return 0x00E2; /* Mute */
    case BLE_HID_MEDIA_STOP:       return 0x00B7; /* Stop */
    default: return 0;
    }
}

esp_err_t ble_hid_send_media(ble_hid_media_command_t command)
{
    const uint16_t usage = consumer_usage(command);
    if (usage == 0) return ESP_ERR_INVALID_ARG;
    lock();
    if (!ble_hid_is_connected()) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t report[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
    esp_err_t err = esp_hidd_dev_input_set(s_hid_dev, 0, 3, report, sizeof(report));
    if (err == ESP_OK) {
        /* Give the host a brief key-down interval, then explicitly release the Consumer key. */
        vTaskDelay(pdMS_TO_TICKS(25));
        report[0] = 0;
        report[1] = 0;
        err = esp_hidd_dev_input_set(s_hid_dev, 0, 3, report, sizeof(report));
    }
    unlock();
    return err;
}

void ble_hid_release_all(void)
{
    lock();
    s_ptt_pressed = false;
    if (ble_hid_is_connected()) {
        uint8_t kb[8] = { 0 };
        uint8_t cc[2] = { 0 };
        (void)esp_hidd_dev_input_set(s_hid_dev, 0, 1, kb, sizeof(kb));
        (void)esp_hidd_dev_input_set(s_hid_dev, 0, 3, cc, sizeof(cc));
    }
    unlock();
}
