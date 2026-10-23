#include "esp_err.h"
#include "ble_hid.h"
#include "media_link.h"

esp_err_t media_link_start(void)
{
    return ble_hid_start();
}

/* The shared HID peripheral stays advertising across media/PTT mode switches. */
void media_link_stop(void)
{
}

esp_err_t media_link_send_command(media_command_t command)
{
    switch (command) {
    case MEDIA_COMMAND_PLAY_PAUSE: return ble_hid_send_media(BLE_HID_MEDIA_PLAY_PAUSE);
    case MEDIA_COMMAND_PREVIOUS:   return ble_hid_send_media(BLE_HID_MEDIA_PREVIOUS);
    case MEDIA_COMMAND_NEXT:       return ble_hid_send_media(BLE_HID_MEDIA_NEXT);
    case MEDIA_COMMAND_VOLUME_UP:  return ble_hid_send_media(BLE_HID_MEDIA_VOLUME_UP);
    case MEDIA_COMMAND_VOLUME_DOWN:return ble_hid_send_media(BLE_HID_MEDIA_VOLUME_DOWN);
    case MEDIA_COMMAND_MUTE:      return ble_hid_send_media(BLE_HID_MEDIA_MUTE);
    case MEDIA_COMMAND_STOP:      return ESP_ERR_NOT_SUPPORTED;
    default: return ESP_ERR_INVALID_ARG;
    }
}
