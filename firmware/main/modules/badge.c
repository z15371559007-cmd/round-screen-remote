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
#include "badge_format.h"
#include "board_port.h"

static const char *TAG = "badge";

#define FACE        BADGE_FACE_PIXELS
#define FRAME_BYTES (FACE * FACE * 2)
#define UPLOAD_CHUNK 16384u
#define ERASE_CHUNK (64u * 1024u)
#define RECEIVE_IDLE_TIMEOUT_US (30LL * 1000 * 1000)
#define RECEIVE_TOTAL_TIMEOUT_US (5LL * 60 * 1000 * 1000)

extern const char badge_upload_html[] asm("_binary_upload_html_start");

static const esp_partition_t *s_part;
static SemaphoreHandle_t s_idx_lock;
static SemaphoreHandle_t s_upload_lock;
static SemaphoreHandle_t s_start_lock;
static uint32_t *s_offsets;            /* frame offsets (points at the u32 length) */
static uint32_t s_count;
static uint16_t s_fps = 10;
static volatile bool s_valid;
static volatile bool s_active;
static volatile bool s_starting;
static volatile bool s_start_error;
static volatile bool s_receiving;
static volatile int s_progress;
static volatile uint32_t s_gen;
static volatile uint32_t s_clip_gen;
static uint8_t *s_jpeg;
static uint8_t *s_pix[2];
static uint8_t *s_validate_pix;       /* separate from LVGL-owned frame buffers */
static lv_image_dsc_t s_dsc[2];
static bool s_net_ready;
static bool s_wifi_on;
static httpd_handle_t s_httpd;

static bool init_once(void);
static void start_wifi(void);

/* Called with s_idx_lock held. v1 is read-only compatibility; new uploads use v2. */
static bool scan_clip(const uint8_t *header_override, size_t wire_size, bool validate_jpegs)
{
    s_valid = false;
    s_count = 0;
    if (!s_part || !s_offsets) return false;

    uint8_t header[BADGE_V2_HEADER_BYTES] = {0};
    if (header_override) {
        memcpy(header, header_override, sizeof header);
    } else if (esp_partition_read(s_part, 0, header, sizeof header) != ESP_OK) {
        return false;
    }

    badge_clip_info_t info;
    if (!badge_parse_header(header, sizeof header, s_part->size, &info)) {
        ESP_LOGI(TAG, "no valid clip header");
        return false;
    }
    if (info.version == BADGE_V2_VERSION &&
        (wire_size != info.wire_header_bytes + info.data_bytes ||
         info.flash_data_offset + info.data_bytes > s_part->size)) {
        ESP_LOGW(TAG, "clip size mismatch");
        return false;
    }

    size_t off = info.flash_data_offset;
    const size_t end = info.version == BADGE_V2_VERSION
        ? info.flash_data_offset + info.data_bytes : s_part->size;
    for (uint32_t i = 0; i < info.frame_count; ++i) {
        uint8_t length_bytes[4];
        if (off > end || end - off < 4u ||
            esp_partition_read(s_part, off, length_bytes, sizeof length_bytes) != ESP_OK) {
            ESP_LOGW(TAG, "truncated frame length at %lu", (unsigned long)i);
            return false;
        }
        const uint32_t jpeg_size = badge_rd32(length_bytes);
        size_t span = 0;
        if (!badge_frame_span(jpeg_size, end - off, &span)) {
            ESP_LOGW(TAG, "invalid frame span at %lu", (unsigned long)i);
            return false;
        }
        s_offsets[i] = (uint32_t)off;
        off += span;
    }
    if (info.version == BADGE_V2_VERSION && off != end) {
        ESP_LOGW(TAG, "unexpected trailing bytes in clip");
        return false;
    }
    if (validate_jpegs) {
        for (uint32_t i = 0; i < info.frame_count; ++i) {
            uint8_t length_bytes[4];
            if (esp_partition_read(s_part, s_offsets[i], length_bytes, sizeof length_bytes) != ESP_OK)
                return false;
            const uint32_t jpeg_size = badge_rd32(length_bytes);
            if (esp_partition_read(s_part, s_offsets[i] + 4u, s_jpeg, jpeg_size) != ESP_OK)
                return false;
            if (!badge_jpeg_has_expected_dimensions(s_jpeg, jpeg_size)) {
                ESP_LOGW(TAG, "unsupported JPEG dimensions at frame %lu", (unsigned long)i);
                return false;
            }
            esp_jpeg_image_cfg_t cfg = {
                .indata = s_jpeg, .indata_size = jpeg_size,
                .outbuf = s_validate_pix, .outbuf_size = FRAME_BYTES,
                .out_format = JPEG_IMAGE_FORMAT_RGB565, .out_scale = JPEG_IMAGE_SCALE_0,
            };
            esp_jpeg_image_output_t out;
            if (esp_jpeg_decode(&cfg, &out) != ESP_OK || out.width != FACE || out.height != FACE) {
                ESP_LOGW(TAG, "JPEG decode validation failed at frame %lu", (unsigned long)i);
                return false;
            }
            if ((i & 3u) == 3u) vTaskDelay(1);
        }
    }

    s_count = info.frame_count;
    s_fps = info.fps;
    return true;
}

static void player_task(void *arg)
{
    const uint32_t gen = (uint32_t)(uintptr_t)arg;

    /* Entry is called from the LVGL button event. Keep PSRAM allocation, flash scan,
       and Wi-Fi/AP startup out of that event task: all of them can take noticeable time.
       Serialize first-time initialization if the user leaves and re-enters quickly. */
    if (!s_start_lock) goto done;
    xSemaphoreTake(s_start_lock, portMAX_DELAY);
    if (!s_active || s_gen != gen) {
        xSemaphoreGive(s_start_lock);
        goto done;
    }
    if (!init_once()) {
        if (s_gen == gen) {
            s_start_error = true;
            s_starting = false;
            ESP_LOGE(TAG, "badge startup failed during player initialization");
        }
        xSemaphoreGive(s_start_lock);
        goto done;
    }
    if (s_active && s_gen == gen) start_wifi();
    if (s_gen == gen) s_starting = false;
    if (!s_active && s_wifi_on) {
        esp_wifi_stop();
        s_wifi_on = false;
    }
    xSemaphoreGive(s_start_lock);
    if (!s_active || s_gen != gen) goto done;

    uint32_t i = 0;
    uint32_t clip_gen = s_clip_gen;
    int back = 0;
    while (s_active && s_gen == gen) {
        if (s_receiving || !s_valid) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (clip_gen != s_clip_gen) { clip_gen = s_clip_gen; i = 0; back = 0; }
        const int64_t t0 = esp_timer_get_time();
        bool ok = false;
        xSemaphoreTake(s_idx_lock, portMAX_DELAY);
        if (s_valid && s_count) {
            if (i >= s_count) i = 0;
            uint8_t l[4];
            if (esp_partition_read(s_part, s_offsets[i], l, 4) == ESP_OK) {
                uint32_t len = badge_rd32(l);
                if (len <= BADGE_MAX_JPEG_BYTES && esp_partition_read(s_part, s_offsets[i] + 4, s_jpeg, len) == ESP_OK &&
                    badge_jpeg_has_expected_dimensions(s_jpeg, len)) {
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
        const int64_t t1 = esp_timer_get_time();
        if (ok && s_active && s_gen == gen && !s_receiving) {
            board_port_badge_show_frame(&s_dsc[back]);
            back ^= 1;
        }
        const int64_t t2 = esp_timer_get_time();
        if ((i % 50) == 1) {
            ESP_LOGI(TAG, "frame %lu: read+decode %lld ms, render %lld ms, ok=%d",
                     (unsigned long)i, (t1 - t0) / 1000, (t2 - t1) / 1000, ok);
        }
        int64_t left_ms = (1000 / fps) - (t2 - t0) / 1000;
        /* Always leave real idle time so LVGL and IDLE tasks run (no task_wdt). */
        vTaskDelay(pdMS_TO_TICKS(left_ms > 20 ? left_ms : 20));
    }
done:
    if (s_gen == gen) {
        s_active = false;
        s_starting = false;
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
    const size_t max_wire = s_part && s_part->size > BADGE_V2_DATA_OFFSET
        ? s_part->size - BADGE_V2_DATA_OFFSET + BADGE_V2_HEADER_BYTES : 0;
    snprintf(buf, sizeof buf, "{\"max\":%lu,\"frames\":%lu,\"fps\":%u}",
             (unsigned long)max_wire, (unsigned long)(s_valid ? s_count : 0), s_fps);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t upload_post(httpd_req_t *req)
{
    if (!s_part || !s_idx_lock || !s_upload_lock) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    const size_t total = req->content_len;
    const size_t max_wire = s_part->size > BADGE_V2_DATA_OFFSET
        ? s_part->size - BADGE_V2_DATA_OFFSET + BADGE_V2_HEADER_BYTES : 0;
    if (total < BADGE_V2_HEADER_BYTES + 68u || total > max_wire) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid upload size");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(s_upload_lock, 0) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "upload already in progress");
        return ESP_FAIL;
    }

    uint8_t *buf = heap_caps_malloc(UPLOAD_CHUNK, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        xSemaphoreGive(s_upload_lock);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint8_t header[BADGE_V2_HEADER_BYTES] = {0};
    size_t header_got = 0;
    size_t received = 0;
    size_t erased = 0;
    esp_err_t err = ESP_OK;
    bool committed = false;
    const int64_t started_at = esp_timer_get_time();
    int64_t last_rx = started_at;

    /* s_receiving gates the player. Taking this lock drains any in-flight frame read/decode. */
    s_receiving = true;
    s_progress = 0;
    xSemaphoreTake(s_idx_lock, portMAX_DELAY);
    s_valid = false;
    s_count = 0;
    xSemaphoreGive(s_idx_lock);

    /* Erase the sector containing the commit header first. Until the final header write,
       boot and the player both treat this slot as empty, even after reset or disconnect. */
    size_t first_erase = s_part->size < ERASE_CHUNK ? s_part->size : ERASE_CHUNK;
    if (first_erase < BADGE_V2_DATA_OFFSET) {
        err = ESP_ERR_INVALID_SIZE;
        ESP_LOGE(TAG, "badge partition too small for transactional upload");
    } else if ((err = esp_partition_erase_range(s_part, 0, first_erase)) != ESP_OK) {
        ESP_LOGE(TAG, "initial erase failed: %s", esp_err_to_name(err));
    } else {
        erased = first_erase;
        vTaskDelay(1);
    }

    ESP_LOGI(TAG, "receiving v2 clip: %u bytes", (unsigned)total);
    while (err == ESP_OK && received < total) {
        if (esp_timer_get_time() - started_at > RECEIVE_TOTAL_TIMEOUT_US) {
            err = ESP_ERR_TIMEOUT;
            break;
        }
        const size_t want = total - received < UPLOAD_CHUNK ? total - received : UPLOAD_CHUNK;
        int r = httpd_req_recv(req, (char *)buf, want);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            if (esp_timer_get_time() - last_rx > RECEIVE_IDLE_TIMEOUT_US) err = ESP_ERR_TIMEOUT;
            continue;
        }
        if (r <= 0) { err = ESP_FAIL; break; }
        last_rx = esp_timer_get_time();

        size_t pos = 0;
        const size_t chunk_end = received + (size_t)r;
        if (received < sizeof header) {
            const size_t take = chunk_end < sizeof header ? chunk_end - received : sizeof header - received;
            memcpy(header + received, buf, take);
            header_got += take;
            pos += take;
        }
        if (pos < (size_t)r) {
            const size_t wire_data_off = received + pos - BADGE_V2_HEADER_BYTES;
            const size_t flash_off = BADGE_V2_DATA_OFFSET + wire_data_off;
            const size_t data_len = (size_t)r - pos;
            const size_t target_end = flash_off + data_len;
            while (erased < target_end) {
                size_t chunk = s_part->size - erased < ERASE_CHUNK ? s_part->size - erased : ERASE_CHUNK;
                if (!chunk || (err = esp_partition_erase_range(s_part, erased, chunk)) != ESP_OK) break;
                erased += chunk;
                vTaskDelay(1);
            }
            if (err != ESP_OK) break;
            if ((err = esp_partition_write(s_part, flash_off, buf + pos, data_len)) != ESP_OK) break;
        }
        received = chunk_end;
        s_progress = (int)(received * 100u / total);
        vTaskDelay(1); /* flash operations and Wi-Fi must leave time for idle/LVGL tasks */
    }

    if (err == ESP_OK && (received != total || header_got != sizeof header)) err = ESP_ERR_INVALID_SIZE;
    badge_clip_info_t info;
    if (err == ESP_OK && (!badge_parse_header(header, sizeof header, s_part->size, &info) ||
                          info.version != BADGE_V2_VERSION ||
                          info.wire_header_bytes + info.data_bytes != total)) {
        err = ESP_ERR_INVALID_ARG;
    }
    if (err == ESP_OK) {
        xSemaphoreTake(s_idx_lock, portMAX_DELAY);
        const bool valid = scan_clip(header, total, true);
        if (!valid) {
            err = ESP_ERR_INVALID_RESPONSE;
        } else if ((err = esp_partition_write(s_part, 0, header, sizeof header)) == ESP_OK) {
            /* Publish the clip only after every frame decoded and the commit header is stored. */
            s_valid = true;
            s_clip_gen++;
            s_progress = 100;
            committed = true;
        }
        xSemaphoreGive(s_idx_lock);
    }

    free(buf);
    s_receiving = false;
    xSemaphoreGive(s_upload_lock);
    if (!committed) {
        s_valid = false;
        s_count = 0;
        ESP_LOGW(TAG, "upload rejected (%s); no clip committed", esp_err_to_name(err));
        httpd_resp_send_err(req, err == ESP_ERR_TIMEOUT ? HTTPD_408_REQ_TIMEOUT : HTTPD_400_BAD_REQUEST,
                            "video invalid or upload interrupted; no clip committed");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static void start_http(void)
{
    if (s_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 12288;
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
    s_upload_lock = xSemaphoreCreateMutex();
    if (!s_idx_lock || !s_upload_lock) return false;
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "badge");
    if (!s_part) ESP_LOGE(TAG, "no 'badge' partition");
    s_offsets = heap_caps_malloc(BADGE_MAX_FRAMES * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    s_jpeg = heap_caps_malloc(BADGE_MAX_JPEG_BYTES, MALLOC_CAP_SPIRAM);
    s_validate_pix = heap_caps_aligned_alloc(64, FRAME_BYTES, MALLOC_CAP_SPIRAM);
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
    if (!s_offsets || !s_jpeg || !s_validate_pix || !s_pix[0] || !s_pix[1]) {
        ESP_LOGE(TAG, "out of PSRAM for player");
        free(s_jpeg); s_jpeg = NULL;
        return false;
    }
    xSemaphoreTake(s_idx_lock, portMAX_DELAY);
    s_valid = scan_clip(NULL, 0, false);
    xSemaphoreGive(s_idx_lock);
    return true;
}

esp_err_t badge_enter(void)
{
    if (!s_start_lock) s_start_lock = xSemaphoreCreateMutex();
    if (!s_start_lock) {
        s_start_error = true;
        return ESP_ERR_NO_MEM;
    }
    s_start_error = false;
    s_starting = true;
    s_active = true;
    const uint32_t gen = ++s_gen;
    /* The worker performs PSRAM allocation/partition scan and starts Wi-Fi. */
    if (xTaskCreatePinnedToCore(player_task, "badge_play", 16384, (void *)(uintptr_t)gen, 3, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "player task create failed");
        s_active = false;
        s_starting = false;
        s_start_error = true;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void badge_exit(void)
{
    s_active = false;
    s_starting = false;
    s_gen++;
    if (s_wifi_on && !s_receiving) {
        esp_wifi_stop();
        s_wifi_on = false;
        ESP_LOGI(TAG, "hotspot off");
    }
}

badge_state_t badge_state(void)
{
    if (s_starting) return BADGE_STATE_STARTING;
    if (s_start_error) return BADGE_STATE_ERROR;
    if (s_receiving) return BADGE_STATE_RECEIVING;
    if (!s_part || !s_jpeg) return BADGE_STATE_ERROR;
    return s_valid ? BADGE_STATE_PLAYING : BADGE_STATE_NO_VIDEO;
}

int badge_progress(void) { return s_progress; }
