#pragma once

/*
 * Backend API for the LVGL UI (board/). Include this one header from UI code.
 * All functions are safe to call from LVGL event callbacks (they do not touch LVGL).
 * Callbacks registered here run on backend tasks: take board_port_ui_lock() before
 * touching LVGL inside them (or just set a flag and refresh from an lv_timer).
 *
 * PTT (hold F13 over BLE HID)          -> push_to_talk.h
 *   ptt_press() / ptt_release()        touch button down / up (LV_EVENT_PRESSED / RELEASED|PRESS_LOST)
 *   ptt_is_held() / ptt_is_ready()     key down on host? / page active + BLE + key configured
 *   ptt_set_state_cb(cb, ctx)          notified on hold/release incl. forced (disconnect, watchdog, mode exit)
 *
 * Playback (BLE HID consumer keys)     -> media_remote.h
 *   media_remote_play_pause/previous/next/volume_up/volume_down/mute/stop()
 *   media_remote_is_connected()
 *
 * BLE link                             -> ble_hid.h
 *   ble_hid_is_connected(), ble_hid_register_conn_cb(cb, ctx), ble_hid_ptt_key_configured()
 *
 * Now playing (STUB: placeholder data) -> host_link.h
 *   host_link_get_now_playing(&np), host_link_register_now_playing_cb(cb, ctx),
 *   host_link_get_state(), host_link_is_stub()   (np.placeholder == true => not real data)
 *
 * Pet voice (STUB)                     -> pet_voice_backend.h
 *   pet_voice_backend_available(), pet_voice_backend_is_listening()
 *
 * Pages                                -> mode_router.h: mode_router_switch_to(), mode_router_current()
 */

#include "push_to_talk.h"
#include "media_remote.h"
#include "ble_hid.h"
#include "host_link.h"
#include "pet_voice_backend.h"
#include "mode_router.h"
