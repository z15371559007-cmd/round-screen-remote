#pragma once

/*
 * Host link: Wi-Fi WebSocket bridge to the computer (host/omarchy-bridge) that supplies
 * "now playing" metadata and richer commands (seek / absolute volume).
 *
 * STATUS: interface + STUB only (CONFIG_ROUND_REMOTE_HOST_LINK_BACKEND_STUB). The stub never
 * opens a socket; it publishes one placeholder now-playing record (or an animated demo track
 * when CONFIG_ROUND_REMOTE_HOST_LINK_STUB_DEMO=y) so the UI can be built against real types.
 * Playback *control* already works without this link via BLE HID (media_remote_*).
 *
 * Wire protocol (protocol v1, JSON over ws://<host>:<port>/ws, mirrors
 * host/omarchy-bridge/src/round_bridge/protocol.py + playerctl.py):
 *   device -> host  {"type":"hello","token":"...","device_id":"..."}
 *   host -> device  {"type":"hello_ack","protocol_version":1}
 *   host -> device  {"type":"state","status":"playing|paused|stopped|unavailable",
 *                    "title":"","artist":"","album":"","position_ms":int|null,
 *                    "duration_ms":int|null,"artwork_available":bool}      (polled ~1 s)
 *   device -> host  {"type":"command","id":"<=64 chars","command":
 *                    "play_pause|previous|next|stop|seek_to|set_volume","value":num?}
 *                    seek_to value = seconds 0..86400, set_volume value = 0..1
 *   host -> device  {"type":"command_result","id":"...","ok":bool,"error":str|null}
 *   host -> device  {"type":"error","code":"..."}
 * The bridge does not send volume or cover art yet; those fields stay -1 / 0 until it does.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define HOST_LINK_PROTOCOL_VERSION 1
#define HOST_LINK_TEXT_MAX 128      /* UTF-8 bytes incl. NUL; longer strings are truncated */

typedef enum {
    HOST_LINK_STATE_STOPPED = 0,    /* host_link_start() not called / stopped */
    HOST_LINK_STATE_STUB,           /* stub backend: placeholder data only */
    HOST_LINK_STATE_CONNECTING,     /* real backend: Wi-Fi / socket / hello in progress */
    HOST_LINK_STATE_CONNECTED,      /* real backend: hello_ack received */
    HOST_LINK_STATE_ERROR,          /* real backend: auth failed / unreachable (retrying) */
} host_link_state_t;

typedef enum {
    HOST_LINK_PLAYBACK_UNAVAILABLE = 0,
    HOST_LINK_PLAYBACK_PLAYING,
    HOST_LINK_PLAYBACK_PAUSED,
    HOST_LINK_PLAYBACK_STOPPED,
} host_link_playback_t;

typedef struct {
    char title[HOST_LINK_TEXT_MAX];
    char artist[HOST_LINK_TEXT_MAX];
    char album[HOST_LINK_TEXT_MAX];
    int32_t position_ms;            /* -1 = unknown */
    int32_t duration_ms;            /* -1 = unknown */
    int16_t volume_pct;             /* 0..100, -1 = unknown */
    bool playing;                   /* convenience: status == PLAYING */
    host_link_playback_t status;
    bool artwork_available;         /* host has art; transfer not implemented yet */
    uint32_t cover_handle;          /* 0 = no cover. Opaque id for a future cover fetch API */
    bool placeholder;               /* true = stub / not real host data, show as such */
    uint32_t seq;                   /* increments on every update */
} host_link_now_playing_t;

typedef enum {
    HOST_LINK_CMD_PLAY_PAUSE = 0,
    HOST_LINK_CMD_PREVIOUS,
    HOST_LINK_CMD_NEXT,
    HOST_LINK_CMD_STOP,
    HOST_LINK_CMD_SEEK_TO,          /* value: seconds */
    HOST_LINK_CMD_SET_VOLUME,       /* value: 0.0..1.0 */
} host_link_command_t;

typedef struct {
    const char *host;               /* NULL -> CONFIG_ROUND_REMOTE_HOST_LINK_HOST */
    uint16_t port;                  /* 0 -> CONFIG_ROUND_REMOTE_HOST_LINK_PORT */
    const char *token;              /* NULL -> CONFIG_ROUND_REMOTE_HOST_LINK_TOKEN (never commit it) */
    const char *device_id;          /* NULL -> "round-remote" */
} host_link_config_t;

/* Callbacks run on the host-link task (stub: esp_timer task). Copy the data you need and take
 * board_port_ui_lock() before touching LVGL. Up to 4 of each; same (cb, ctx) registered once. */
typedef void (*host_link_now_playing_cb_t)(const host_link_now_playing_t *now_playing, void *ctx);
typedef void (*host_link_state_cb_t)(host_link_state_t state, void *ctx);

esp_err_t host_link_start(const host_link_config_t *config);   /* NULL = Kconfig defaults */
void host_link_stop(void);
host_link_state_t host_link_get_state(void);
bool host_link_is_stub(void);

/* Copies the latest snapshot. Always succeeds after start (placeholder in stub). */
esp_err_t host_link_get_now_playing(host_link_now_playing_t *out);

esp_err_t host_link_register_now_playing_cb(host_link_now_playing_cb_t cb, void *ctx);
esp_err_t host_link_register_state_cb(host_link_state_cb_t cb, void *ctx);

/* Stub: ESP_ERR_NOT_SUPPORTED (use media_remote_* over BLE instead). */
esp_err_t host_link_send_command(host_link_command_t command, float value);

/* ---- protocol helpers (backend-independent, host_link_protocol.c) ---- */
const char *host_link_command_name(host_link_command_t command);
/* Return bytes written (excl. NUL) or -1 if buf too small / bad args. */
int host_link_format_hello(char *buf, size_t len, const char *token, const char *device_id);
int host_link_format_command(char *buf, size_t len, const char *request_id,
                             host_link_command_t command, float value);
host_link_playback_t host_link_playback_from_string(const char *status);
