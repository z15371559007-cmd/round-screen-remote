#pragma once

/* Pure, host-testable parser helpers for the badge clip on-flash format. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BADGE_FACE_PIXELS 466u
#define BADGE_MAX_FPS 30u
#define BADGE_MAX_DURATION_SECONDS 10u
#define BADGE_MAX_FRAMES (BADGE_MAX_FPS * BADGE_MAX_DURATION_SECONDS)
#define BADGE_MAX_JPEG_BYTES (256u * 1024u)
#define BADGE_V1_HEADER_BYTES 16u
#define BADGE_V2_HEADER_BYTES 24u
#define BADGE_V2_DATA_OFFSET 4096u

#define BADGE_V1_VERSION 1u
#define BADGE_V2_VERSION 2u

typedef struct {
    uint16_t version;
    uint16_t fps;
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint32_t data_bytes;       /* v2 only: exact bytes after the wire header */
    size_t wire_header_bytes;
    size_t flash_data_offset;
} badge_clip_info_t;

static inline uint16_t badge_rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t badge_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint32_t badge_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

/* The v2 header uses a CRC and keeps frame data beyond an erase-sector boundary.
 * The header is written last, so interrupted uploads are never accepted as clips. */
static inline bool badge_parse_header(const uint8_t *header, size_t header_size,
                                      size_t partition_size, badge_clip_info_t *out)
{
    if (!header || !out || header_size < BADGE_V1_HEADER_BYTES ||
        memcmp(header, "ORBV", 4) != 0) return false;

    badge_clip_info_t info = {0};
    info.version = badge_rd16(header + 4);
    info.fps = badge_rd16(header + 6);
    info.width = badge_rd16(header + 8);
    info.height = badge_rd16(header + 10);
    info.frame_count = badge_rd32(header + 12);

    if (info.version == BADGE_V1_VERSION) {
        info.wire_header_bytes = BADGE_V1_HEADER_BYTES;
        info.flash_data_offset = BADGE_V1_HEADER_BYTES;
    } else if (info.version == BADGE_V2_VERSION) {
        if (header_size < BADGE_V2_HEADER_BYTES || partition_size < BADGE_V2_DATA_OFFSET ||
            badge_rd32(header + 20) != badge_crc32(header, 20)) return false;
        info.data_bytes = badge_rd32(header + 16);
        info.wire_header_bytes = BADGE_V2_HEADER_BYTES;
        info.flash_data_offset = BADGE_V2_DATA_OFFSET;
        if (info.data_bytes < 68u || info.data_bytes > partition_size - info.flash_data_offset)
            return false;
    } else {
        return false;
    }

    if (info.fps < 1u || info.fps > BADGE_MAX_FPS ||
        info.width != BADGE_FACE_PIXELS || info.height != BADGE_FACE_PIXELS ||
        info.frame_count < 1u || info.frame_count > BADGE_MAX_FRAMES ||
        info.frame_count > (uint32_t)info.fps * BADGE_MAX_DURATION_SECONDS) return false;

    *out = info;
    return true;
}


/* Validate the bounded baseline JPEG SOF before calling esp_jpeg_decode(). The
 * decoder computes width*height in uint32_t before checking outbuf_size; checking
 * dimensions here prevents hostile/truncated headers from wrapping that product. */
static inline bool badge_jpeg_has_expected_dimensions(const uint8_t *data, size_t size)
{
    if (!data || size < 4u || data[0] != 0xffu || data[1] != 0xd8u) return false;
    size_t pos = 2u;
    while (pos < size) {
        if (data[pos++] != 0xffu) return false;
        while (pos < size && data[pos] == 0xffu) ++pos;
        if (pos >= size) return false;
        const uint8_t marker = data[pos++];
        if (marker == 0xd9u || marker == 0xdau) return false; /* no SOF before scan/end */
        if (marker == 0xd8u || marker == 0x01u || (marker >= 0xd0u && marker <= 0xd7u)) continue;
        if (size - pos < 2u) return false;
        const size_t segment_len = ((size_t)data[pos] << 8) | data[pos + 1u];
        if (segment_len < 2u || segment_len > size - pos) return false;
        if (marker == 0xc0u) {
            if (segment_len < 8u) return false;
            const uint8_t *sof = data + pos + 2u;
            const uint8_t components = sof[5];
            if (sof[0] != 8u || components != 3u || segment_len != 8u + 3u * components)
                return false;
            const uint16_t height = (uint16_t)(((uint16_t)sof[1] << 8) | sof[2]);
            const uint16_t width = (uint16_t)(((uint16_t)sof[3] << 8) | sof[4]);
            return width == BADGE_FACE_PIXELS && height == BADGE_FACE_PIXELS;
        }
        /* Other SOF modes (e.g. progressive) aren't accepted by the bundled decoder. */
        if ((marker >= 0xc1u && marker <= 0xcfu) && marker != 0xc4u && marker != 0xc8u && marker != 0xccu)
            return false;
        pos += segment_len;
    }
    return false;
}

/* `remaining` counts the frame length word plus bytes left in the clip payload. */
static inline bool badge_frame_span(uint32_t jpeg_size, size_t remaining, size_t *span)
{
    if (!span || remaining < 4u || jpeg_size < 64u || jpeg_size > BADGE_MAX_JPEG_BYTES)
        return false;
    const size_t padded = ((size_t)jpeg_size + 3u) & ~(size_t)3u;
    if (padded > remaining - 4u) return false;
    *span = 4u + padded;
    return true;
}
