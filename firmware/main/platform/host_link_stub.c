/* Stub host link: no Wi-Fi, no socket. Publishes placeholder now-playing data. */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host_link.h"

static const char *TAG = "host_link";

#define CB_MAX 4
static struct { host_link_now_playing_cb_t cb; void *ctx; } s_np_cbs[CB_MAX];
static struct { host_link_state_cb_t cb; void *ctx; } s_st_cbs[CB_MAX];
static host_link_state_t s_state = HOST_LINK_STATE_STOPPED;
static host_link_now_playing_t s_np;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buf;
static esp_timer_handle_t s_tick;

static void lock(void)   { if (!s_lock) s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf); xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void copy_text(char *dst, const char *src)
{
    snprintf(dst, HOST_LINK_TEXT_MAX, "%s", src ? src : "");
}

static void fill_placeholder(host_link_now_playing_t *np)
{
    memset(np, 0, sizeof(*np));
#if CONFIG_ROUND_REMOTE_HOST_LINK_STUB_DEMO
    copy_text(np->title, "示例歌曲");
    copy_text(np->artist, "示例歌手");
    copy_text(np->album, "示例专辑");
    np->duration_ms = 215000;
    np->position_ms = (int32_t)((esp_timer_get_time() / 1000) % 215000);
    np->volume_pct = 60;
    np->status = HOST_LINK_PLAYBACK_PLAYING;
    np->playing = true;
#else
    copy_text(np->title, "未连接电脑");
    copy_text(np->artist, "Wi-Fi 桥接未实现");
    copy_text(np->album, "");
    np->duration_ms = -1;
    np->position_ms = -1;
    np->volume_pct = -1;
    np->status = HOST_LINK_PLAYBACK_UNAVAILABLE;
    np->playing = false;
#endif
    np->artwork_available = false;
    np->cover_handle = 0;
    np->placeholder = true;
}

static void publish(void)
{
    host_link_now_playing_t copy;
    lock();
    uint32_t seq = s_np.seq + 1;
    fill_placeholder(&s_np);
    s_np.seq = seq;
    copy = s_np;
    unlock();
    for (int i = 0; i < CB_MAX; i++) {
        if (s_np_cbs[i].cb) s_np_cbs[i].cb(&copy, s_np_cbs[i].ctx);
    }
}

static void set_state(host_link_state_t st)
{
    s_state = st;
    for (int i = 0; i < CB_MAX; i++) {
        if (s_st_cbs[i].cb) s_st_cbs[i].cb(st, s_st_cbs[i].ctx);
    }
}

static void tick(void *arg)
{
    (void)arg;
    publish();
}

esp_err_t host_link_start(const host_link_config_t *config)
{
    (void)config;
    if (s_state != HOST_LINK_STATE_STOPPED) return ESP_OK;
    ESP_LOGW(TAG, "Host link is a STUB (no Wi-Fi bridge yet); publishing placeholder now-playing");
    set_state(HOST_LINK_STATE_STUB);
    publish();
#if CONFIG_ROUND_REMOTE_HOST_LINK_STUB_DEMO
    if (!s_tick) {
        const esp_timer_create_args_t args = { .callback = tick, .name = "host_link_demo" };
        if (esp_timer_create(&args, &s_tick) != ESP_OK) s_tick = NULL;
    }
    if (s_tick) (void)esp_timer_start_periodic(s_tick, 1000000ULL);
#else
    (void)tick;
#endif
    return ESP_OK;
}

void host_link_stop(void)
{
    if (s_tick) (void)esp_timer_stop(s_tick);
    if (s_state != HOST_LINK_STATE_STOPPED) set_state(HOST_LINK_STATE_STOPPED);
}

host_link_state_t host_link_get_state(void) { return s_state; }
bool host_link_is_stub(void) { return true; }

esp_err_t host_link_get_now_playing(host_link_now_playing_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    lock();
    if (s_np.seq == 0) fill_placeholder(&s_np);
    *out = s_np;
    unlock();
    return ESP_OK;
}

esp_err_t host_link_register_now_playing_cb(host_link_now_playing_cb_t cb, void *ctx)
{
    if (!cb) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < CB_MAX; i++) if (s_np_cbs[i].cb == cb && s_np_cbs[i].ctx == ctx) return ESP_OK;
    for (int i = 0; i < CB_MAX; i++) {
        if (!s_np_cbs[i].cb) { s_np_cbs[i].ctx = ctx; s_np_cbs[i].cb = cb; return ESP_OK; }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t host_link_register_state_cb(host_link_state_cb_t cb, void *ctx)
{
    if (!cb) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < CB_MAX; i++) if (s_st_cbs[i].cb == cb && s_st_cbs[i].ctx == ctx) return ESP_OK;
    for (int i = 0; i < CB_MAX; i++) {
        if (!s_st_cbs[i].cb) { s_st_cbs[i].ctx = ctx; s_st_cbs[i].cb = cb; return ESP_OK; }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t host_link_send_command(host_link_command_t command, float value)
{
    (void)command;
    (void)value;
    return ESP_ERR_NOT_SUPPORTED;
}
