#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "lvgl.h"

#include "badge.h"
#include "board_port.h"

static const char *TAG = "badge";

#define FACE        466
#define FRAME_BYTES (FACE * FACE * 2)
#define MAX_FRAMES  600
#define MAX_JPEG    (256 * 1024)
#define HDR_SIZE    16

extern const char badge_upload_html[] asm("_binary_upload_html_start");

static const esp_partition_t *s_part;
static SemaphoreHandle_t s_idx_lock;
static uint32_t *s_offsets;            /* frame offsets (points at the u32 length) */
static uint32_t s_count;
static uint16_t s_fps = 10;
static volatile bool s_valid;
static volatile bool s_active;
static volatile bool s_receiving;
static volatile int s_progress;
static volatile uint32_t s_gen;
static uint8_t *s_jpeg;
static uint8_t *s_pix[2];
static lv_image_dsc_t s_dsc[2];
static bool s_net_ready;
static bool s_wifi_on;
static httpd_handle_t s_httpd;

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

/* Called with s_idx_lock held. */
static void scan_clip(void)
{
    s_valid = false;
    s_count = 0;
    if (!s_part || !s_offsets) return;
    uint8_t h[HDR_SIZE];
    if (esp_partition_read(s_part, 0, h, sizeof h) != ESP_OK) return;
    if (memcmp(h, "ORBV", 4) != 0 || rd16(h + 4) != 1) { ESP_LOGI(TAG, "no clip stored"); return; }
    uint16_t fps = rd16(h + 6), w = rd16(h + 8), hh = rd16(h + 10);
    uint32_t n = rd32(h + 12);
    if (fps < 1 || fps > 30 || w != FACE || hh != FACE || n < 1 || n > MAX_FRAMES) {
        ESP_LOGW(TAG, "bad clip header fps=%u %ux%u n=%lu", fps, w, hh, (unsigned long)n);
        return;
    }
    uint32_t off = HDR_SIZE;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t l[4];
        if (off + 4 > s_part->size || esp_partition_read(s_part, off, l, 4) != ESP_OK) return;
        uint32_t len = rd32(l);
        if (len < 64 || len > MAX_JPEG || off + 4 + len > s_part->size) {
            ESP_LOGW(TAG, "bad frame %lu len %lu", (unsigned long)i, (unsigned long)len);
            return;
        }
        s_offsets[i] = off;
        off += 4 + ((len + 3) & ~3u);
    }
    s_count = n;
    s_fps = fps;
    s_valid = true;
    ESP_LOGI(TAG, "clip: %lu frames @ %u fps, %lu bytes", (unsigned long)n, fps, (unsigned long)off);
}

static void player_task(void *arg)
{
    const uint32_t gen = (uint32_t)(uintptr_t)arg;
    uint32_t i = 0;
    int back = 0;
    while (s_active && s_gen == gen) {
        if (s_receiving || !s_valid) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        const int64_t t0 = esp_timer_get_time();
        bool ok = false;
        xSemaphoreTake(s_idx_lock, portMAX_DELAY);
        if (s_valid && s_count) {
            if (i >= s_count) i = 0;
            uint8_t l[4];
            if (esp_partition_read(s_part, s_offsets[i], l, 4) == ESP_OK) {
                uint32_t len = rd32(l);
                if (len <= MAX_JPEG && esp_partition_read(s_part, s_offsets[i] + 4, s_jpeg, len) == ESP_OK) {
                    esp_jpeg_image_cfg_t cfg = {
                        .indata = s_jpeg, .indata_size = len,
                        .outbuf = s_pix[back], .outbuf_size = FRAME_BYTES,
                        .out_format = JPEG_IMAGE_FORMAT_RGB565, .out_scale = JPEG_IMAGE_SCALE_0,
                    };
                    esp_jpeg_image_output_t out;
                    ok = esp_jpeg_decode(&cfg, &out) == ESP_OK && out.width == FACE && out.height == FACE;
                }
            }
            i++;
        }
        const uint16_t fps = s_fps;
        xSemaphoreGive(s_idx_lock);
        if (ok && s_active && s_gen == gen && !s_receiving) {
            board_port_badge_show_frame(&s_dsc[back]);
            back ^= 1;
        }
        int64_t left_ms = (1000 / fps) - (esp_timer_get_time() - t0) / 1000;
        vTaskDelay(pdMS_TO_TICKS(left_ms > 5 ? left_ms : 5));   /* always yield: no task_wdt */
    }
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------- HTTP */

static esp_err_t page_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, badge_upload_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t info_get(httpd_req_t *req)
{
    char buf[96];
    snprintf(buf, sizeof buf, "{\"max\":%lu,\"frames\":%lu,\"fps\":%u}",
             (unsigned long)(s_part ? s_part->size : 0), (unsigned long)(s_valid ? s_count : 0), s_fps);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t upload_post(httpd_req_t *req)
{
    const size_t total = req->content_len;
    if (!s_part || total < HDR_SIZE + 68 || total > s_part->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "size");
        return ESP_FAIL;
    }
    uint8_t *buf = heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }

    xSemaphoreTake(s_idx_lock, portMAX_DELAY);
    s_valid = false;
    xSemaphoreGive(s_idx_lock);
    s_receiving = true;
    s_progress = 0;
    ESP_LOGI(TAG, "receiving clip: %u bytes", (unsigned)total);

    size_t done = 0, erased = 0;
    esp_err_t err = ESP_OK;
    while (done < total) {
        int r = httpd_req_recv(req, (char *)buf, total - done < 16384 ? total - done : 16384);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { err = ESP_FAIL; break; }
        while (erased < done + r) {
            size_t chunk = 64 * 1024;
            if (erased + chunk > s_part->size) chunk = s_part->size - erased;
            if ((err = esp_partition_erase_range(s_part, erased, chunk)) != ESP_OK) break;
            erased += chunk;
        }
        if (err != ESP_OK) break;
        if ((err = esp_partition_write(s_part, done, buf, r)) != ESP_OK) break;
        done += r;
        s_progress = (int)(done * 100 / total);
    }
    free(buf);

    xSemaphoreTake(s_idx_lock, portMAX_DELAY);
    scan_clip();
    xSemaphoreGive(s_idx_lock);
    s_receiving = false;

    if (err != ESP_OK || !s_valid) {
        ESP_LOGW(TAG, "upload failed (%s, valid=%d)", esp_err_to_name(err), s_valid);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad clip");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static void start_http(void)
{
    if (s_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.recv_wait_timeout = 15;
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_httpd, &cfg) != ESP_OK) { ESP_LOGE(TAG, "httpd start failed"); s_httpd = NULL; return; }
    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = page_get },
        { .uri = "/info", .method = HTTP_GET, .handler = info_get },
        { .uri = "/upload", .method = HTTP_POST, .handler = upload_post },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) httpd_register_uri_handler(s_httpd, &uris[i]);
    ESP_LOGI(TAG, "upload page at http://" BADGE_URL "/");
}

static void start_wifi(void)
{
    if (s_wifi_on) return;
    if (!s_net_ready) {
        esp_err_t e = esp_netif_init();
        if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { ESP_LOGE(TAG, "netif init: %s", esp_err_to_name(e)); return; }
        e = esp_event_loop_create_default();
        if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(e)); return; }
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
        if ((e = esp_wifi_init(&init)) != ESP_OK) { ESP_LOGE(TAG, "wifi init: %s", esp_err_to_name(e)); return; }
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        wifi_config_t ap = { 0 };
        strcpy((char *)ap.ap.ssid, BADGE_WIFI_SSID);
        ap.ap.ssid_len = strlen(BADGE_WIFI_SSID);
        strcpy((char *)ap.ap.password, BADGE_WIFI_PASS);
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ap.ap.max_connection = 2;
        ap.ap.channel = 6;
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &ap);
        s_net_ready = true;
    }
    esp_err_t e = esp_wifi_start();
    if (e != ESP_OK) { ESP_LOGE(TAG, "wifi start: %s", esp_err_to_name(e)); return; }
    s_wifi_on = true;
    ESP_LOGI(TAG, "hotspot %s up", BADGE_WIFI_SSID);
    start_http();
}

/* ---------------------------------------------------------------- API */

static bool init_once(void)
{
    if (s_idx_lock) return s_jpeg != NULL;
    s_idx_lock = xSemaphoreCreateMutex();
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "badge");
    if (!s_part) ESP_LOGE(TAG, "no 'badge' partition");
    s_offsets = heap_caps_malloc(MAX_FRAMES * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    s_jpeg = heap_caps_malloc(MAX_JPEG, MALLOC_CAP_SPIRAM);
    for (int i = 0; i < 2; i++) {
        s_pix[i] = heap_caps_aligned_alloc(64, FRAME_BYTES, MALLOC_CAP_SPIRAM);
        if (!s_pix[i]) continue;
        memset(s_pix[i], 0, FRAME_BYTES);
        s_dsc[i] = (lv_image_dsc_t){
            .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                        .w = FACE, .h = FACE, .stride = FACE * 2 },
            .data_size = FRAME_BYTES,
            .data = s_pix[i],
        };
    }
    if (!s_offsets || !s_jpeg || !s_pix[0] || !s_pix[1]) {
        ESP_LOGE(TAG, "out of PSRAM for player");
        free(s_jpeg); s_jpeg = NULL;
        return false;
    }
    xSemaphoreTake(s_idx_lock, portMAX_DELAY);
    scan_clip();
    xSemaphoreGive(s_idx_lock);
    return true;
}

esp_err_t badge_enter(void)
{
    if (!init_once()) return ESP_ERR_NO_MEM;
    s_active = true;
    const uint32_t gen = ++s_gen;
    start_wifi();
    if (xTaskCreatePinnedToCore(player_task, "badge_play", 6144, (void *)(uintptr_t)gen, 3, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "player task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void badge_exit(void)
{
    s_active = false;
    s_gen++;
    if (s_wifi_on && !s_receiving) {
        esp_wifi_stop();
        s_wifi_on = false;
        ESP_LOGI(TAG, "hotspot off");
    }
}

badge_state_t badge_state(void)
{
    if (s_receiving) return BADGE_STATE_RECEIVING;
    if (!s_part || !s_jpeg) return BADGE_STATE_ERROR;
    return s_valid ? BADGE_STATE_PLAYING : BADGE_STATE_NO_VIDEO;
}

int badge_progress(void) { return s_progress; }
