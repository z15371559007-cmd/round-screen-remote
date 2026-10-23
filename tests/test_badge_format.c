#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "badge_format.h"

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void make_v2(uint8_t h[BADGE_V2_HEADER_BYTES], uint16_t fps, uint32_t frames, uint32_t data_bytes)
{
    memset(h, 0, BADGE_V2_HEADER_BYTES);
    memcpy(h, "ORBV", 4); put16(h + 4, BADGE_V2_VERSION); put16(h + 6, fps);
    put16(h + 8, BADGE_FACE_PIXELS); put16(h + 10, BADGE_FACE_PIXELS);
    put32(h + 12, frames); put32(h + 16, data_bytes); put32(h + 20, badge_crc32(h, 20));
}

static uint32_t fuzz_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *state = x;
}

int main(void)
{
    /* Deterministic malformed-input stress: parsers must stay bounded on junk. */
    uint32_t rng = 0x5eed1234u;
    for (unsigned round = 0; round < 20000; ++round) {
        uint8_t bytes[64];
        for (size_t i = 0; i < sizeof bytes; ++i) bytes[i] = (uint8_t)fuzz_next(&rng);
        const size_t jpeg_size = fuzz_next(&rng) % (sizeof bytes + 1u);
        (void)badge_jpeg_has_expected_dimensions(bytes, jpeg_size);
        badge_clip_info_t fuzz_info;
        (void)badge_parse_header(bytes, fuzz_next(&rng) % (sizeof bytes + 1u),
                                 fuzz_next(&rng) % (8u * 1024u * 1024u + 1u), &fuzz_info);
        size_t fuzz_span;
        (void)badge_frame_span(fuzz_next(&rng), fuzz_next(&rng) % 128u, &fuzz_span);
    }
    assert(badge_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926u);
    uint8_t h[BADGE_V2_HEADER_BYTES]; badge_clip_info_t info;
    make_v2(h, 10, 50, 3600);
    assert(badge_parse_header(h, sizeof h, 8u * 1024u * 1024u, &info));
    assert(info.version == BADGE_V2_VERSION && info.frame_count == 50);
    assert(info.flash_data_offset == BADGE_V2_DATA_OFFSET && info.data_bytes == 3600);

    size_t span = 0;
    assert(badge_frame_span(64, 68, &span) && span == 68);
    assert(badge_frame_span(65, 72, &span) && span == 72);
    assert(!badge_frame_span(63, 100, &span));
    assert(!badge_frame_span(BADGE_MAX_JPEG_BYTES + 1, 300000, &span));
    assert(!badge_frame_span(64, 67, &span));

    /* Minimal baseline JPEG header with a bounded 466x466 RGB frame. */
    uint8_t jpeg[24] = {0xff,0xd8, 0xff,0xc0, 0x00,0x11, 8, 0x01,0xd2, 0x01,0xd2, 3,
                        1,0x11,0, 2,0x11,0, 3,0x11,0, 0xff,0xda,0};
    assert(badge_jpeg_has_expected_dimensions(jpeg, sizeof jpeg));
    jpeg[7] = 0xff; jpeg[8] = 0xff; jpeg[9] = 0xff; jpeg[10] = 0xff;
    assert(!badge_jpeg_has_expected_dimensions(jpeg, sizeof jpeg));
    jpeg[8] = 0x01; jpeg[9] = 0xd2; jpeg[6] = 16;
    assert(!badge_jpeg_has_expected_dimensions(jpeg, sizeof jpeg));
    assert(!badge_jpeg_has_expected_dimensions(jpeg, 5));

    uint8_t bad[BADGE_V2_HEADER_BYTES];
    memcpy(bad, h, sizeof bad); bad[20] ^= 1; assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));
    make_v2(bad, 31, 50, 3600); assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));
    make_v2(bad, 10, 101, 3600); assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));
    make_v2(bad, 10, 50, 3600); put16(bad + 8, 640); put32(bad + 20, badge_crc32(bad, 20));
    assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));
    make_v2(bad, 10, 50, 8u*1024u*1024u); assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));
    make_v2(bad, 10, 50, 67); assert(!badge_parse_header(bad, sizeof bad, 8u*1024u*1024u, &info));

    uint8_t v1[BADGE_V1_HEADER_BYTES] = {0};
    memcpy(v1, "ORBV", 4); put16(v1 + 4, BADGE_V1_VERSION); put16(v1 + 6, 10);
    put16(v1 + 8, BADGE_FACE_PIXELS); put16(v1 + 10, BADGE_FACE_PIXELS); put32(v1 + 12, 50);
    assert(badge_parse_header(v1, sizeof v1, 8u*1024u*1024u, &info));
    assert(info.version == BADGE_V1_VERSION && info.flash_data_offset == BADGE_V1_HEADER_BYTES);
    put32(v1 + 12, 101); assert(!badge_parse_header(v1, sizeof v1, 8u*1024u*1024u, &info));

    puts("badge format tests passed");
    return 0;
}
