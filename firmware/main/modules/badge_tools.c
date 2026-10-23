#include "badge_tools.h"

#include <string.h>

static badge_tools_snapshot_t s_state;
static uint64_t s_started_at_us;

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
    s_state.page = (s_state.page == BADGE_TOOL_STOPWATCH) ? BADGE_TOOL_BUBBLE : BADGE_TOOL_STOPWATCH;
}

void badge_tools_primary(uint64_t now_us)
{
    if (s_state.page == BADGE_TOOL_BUBBLE) {
        if (s_state.bubbles_popped < 9999u) ++s_state.bubbles_popped;
        return;
    }
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
