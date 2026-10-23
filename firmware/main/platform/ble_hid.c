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
#include "nvs_flash.h"

#include "ble_hid.h"

static const char *TAG = "ble_hid";
static bool s_started;
static esp_hidd_dev_t *s_hid_dev;
static bool s_ptt_pressed;

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
    0x15, 0x00, 0x25, 0x65, 0x05, 0x07,
    0x19, 0x00, 0x29, 0x65, 0x75, 0x08, 0x95, 0x06,
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
    .version = 0x0100,
    .device_name = "Round Screen Remote",
    .manufacturer_name = "Personal Project",
    .serial_number = "RSR-01",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static void hid_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;
    (void)base;
    esp_hidd_event_data_t *event = (esp_hidd_event_data_t *)event_data;
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT: {
        esp_err_t err = esp_hid_ble_gap_adv_start();
        if (err != ESP_OK) ESP_LOGE(TAG, "BLE advertising start failed: %s", esp_err_to_name(err));
        else ESP_LOGI(TAG, "BLE HID advertising");
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "BLE HID host connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        s_ptt_pressed = false;
        ESP_LOGI(TAG, "BLE HID host disconnected (reason %d)", event->disconnect.reason);
        if (esp_hid_ble_gap_adv_start() != ESP_OK) ESP_LOGW(TAG, "BLE advertising restart failed");
        break;
    default:
        break;
    }
}

esp_err_t ble_hid_start(void)
{
    if (s_started) return ESP_OK;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s); refusing to erase existing data", esp_err_to_name(err));
        return err;
    }

    err = esp_hid_gap_init(HID_DEV_MODE);
    if (err != ESP_OK) return err;

    err = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_device_config.device_name);
    if (err != ESP_OK) return err;

    err = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    if (err != ESP_OK) return err;

    err = esp_hidd_dev_init(&s_device_config, ESP_HID_TRANSPORT_BLE, hid_event_handler, &s_hid_dev);
    if (err != ESP_OK) return err;

    s_started = true;
    ESP_LOGI(TAG, "BLE HID ready; PTT HID usage=0x%02X modifiers=0x%02X (0 means unconfigured)",
             CONFIG_ROUND_REMOTE_PTT_HID_USAGE, CONFIG_ROUND_REMOTE_PTT_HID_MODIFIERS);
    return ESP_OK;
}

bool ble_hid_is_connected(void)
{
    return s_hid_dev != NULL && esp_hidd_dev_connected(s_hid_dev);
}

bool ble_hid_ptt_key_configured(void)
{
    const int usage = CONFIG_ROUND_REMOTE_PTT_HID_USAGE;
    /* Keyboard usages 0x00-0x03 are reserved by the HID usage table. */
    return usage >= 0x04 && usage <= 0x65;
}

static esp_err_t send_keyboard_report(uint8_t modifiers, uint8_t usage)
{
    if (!ble_hid_is_connected()) return ESP_ERR_INVALID_STATE;
    uint8_t report[8] = { modifiers, 0, usage, 0, 0, 0, 0, 0 };
    return esp_hidd_dev_input_set(s_hid_dev, 0, 1, report, sizeof(report));
}

esp_err_t ble_hid_set_pressed(bool pressed)
{
    if (!ble_hid_is_connected()) return ESP_ERR_INVALID_STATE;
    if (!ble_hid_ptt_key_configured()) return ESP_ERR_NOT_SUPPORTED;
    if (pressed == s_ptt_pressed) return ESP_OK;

    esp_err_t err = pressed
        ? send_keyboard_report((uint8_t)CONFIG_ROUND_REMOTE_PTT_HID_MODIFIERS,
                               (uint8_t)CONFIG_ROUND_REMOTE_PTT_HID_USAGE)
        : send_keyboard_report(0, 0);
    if (err == ESP_OK) s_ptt_pressed = pressed;
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
    default: return 0;
    }
}

esp_err_t ble_hid_send_media(ble_hid_media_command_t command)
{
    if (!ble_hid_is_connected()) return ESP_ERR_INVALID_STATE;
    const uint16_t usage = consumer_usage(command);
    if (usage == 0) return ESP_ERR_INVALID_ARG;

    uint8_t report[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
    esp_err_t err = esp_hidd_dev_input_set(s_hid_dev, 0, 3, report, sizeof(report));
    if (err != ESP_OK) return err;

    /* Give the host a brief key-down interval, then explicitly release the Consumer key. */
    vTaskDelay(pdMS_TO_TICKS(25));
    report[0] = 0;
    report[1] = 0;
    return esp_hidd_dev_input_set(s_hid_dev, 0, 3, report, sizeof(report));
}
