#include "badge_tools.h"

#include <string.h>

static badge_tools_snapshot_t s_state;
static uint64_t s_started_at_us;

/* Standard Keyboard/Keypad-page usages. These are one-shot HID keys, not host-specific macros. */
static const badge_tool_key_t s_keys[] = {
    { "ESC",   0x29, 0 },
    { "ENTER", 0x28, 0 },
    { "TAB",   0x2B, 0 },
    { "SPACE", 0x2C, 0 },
    { "LEFT",  0x50, 0 },
    { "DOWN",  0x51, 0 },
    { "UP",    0x52, 0 },
    { "RIGHT", 0x4F, 0 },
};

void badge_tools_enter(void)
{
    /* State is intentionally retained when switching to another remote mode. */
}

void badge_tools_exit(uint64_t now_us)
{
    if (s_state.running) {
        const uint64_t delta = now_us >= s_started_at_us ? now_us - s_started_at_us : 0;
        s_state.elapsed_ms += delta / 1000u;
        s_state.running = false;
    }
}

void badge_tools_next_page(void)
{
    s_state.page = (badge_tool_page_t)(((unsigned)s_state.page + 1u) % 3u);
}

uint8_t badge_tools_key_count(void)
{
    return (uint8_t)(sizeof(s_keys) / sizeof(s_keys[0]));
}

const badge_tool_key_t *badge_tools_key_for_index(uint8_t index)
{
    if (index >= badge_tools_key_count()) return NULL;
    return &s_keys[index];
}

void badge_tools_step_key(int delta)
{
    const int count = badge_tools_key_count();
    if (count == 0) return;
    int next = ((int)s_state.key_index + delta) % count;
    if (next < 0) next += count;
    s_state.key_index = (uint8_t)next;
}

void badge_tools_primary(uint64_t now_us)
{
    if (s_state.page == BADGE_TOOL_BUBBLE) {
        if (s_state.bubbles_popped < 9999u) ++s_state.bubbles_popped;
        return;
    }
    if (s_state.page == BADGE_TOOL_KEY_RING) return;
    if (s_state.running) {
        const uint64_t delta = now_us >= s_started_at_us ? now_us - s_started_at_us : 0;
        s_state.elapsed_ms += delta / 1000u;
        s_state.running = false;
    } else {
        s_started_at_us = now_us;
        s_state.running = true;
    }
}

void badge_tools_reset(uint64_t now_us)
{
    if (s_state.page == BADGE_TOOL_BUBBLE) {
        s_state.bubbles_popped = 0;
        return;
    }
    if (s_state.page == BADGE_TOOL_KEY_RING) {
        s_state.key_index = 0;
        return;
    }
    s_state.elapsed_ms = 0;
    s_state.running = false;
    s_started_at_us = now_us;
}

void badge_tools_snapshot(uint64_t now_us, badge_tools_snapshot_t *out)
{
    if (!out) return;
    *out = s_state;
    if (s_state.running) {
        const uint64_t delta = now_us >= s_started_at_us ? now_us - s_started_at_us : 0;
        out->elapsed_ms += delta / 1000u;
    }
}
