#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BADGE_TOOL_STOPWATCH = 0,
    BADGE_TOOL_BUBBLE,
    BADGE_TOOL_KEY_RING,
} badge_tool_page_t;

typedef struct {
    const char *label;
    uint8_t usage;
    uint8_t modifiers;
} badge_tool_key_t;

typedef struct {
    badge_tool_page_t page;
    bool running;
    uint64_t elapsed_ms;
    uint16_t bubbles_popped;
    uint8_t key_index;
} badge_tools_snapshot_t;

/* Small, RAM-only companion apps inspired by The Badge; no flash layout required. */
void badge_tools_enter(void);
void badge_tools_exit(uint64_t now_us);
void badge_tools_next_page(void);
void badge_tools_step_key(int delta);
uint8_t badge_tools_key_count(void);
const badge_tool_key_t *badge_tools_key_for_index(uint8_t index);
void badge_tools_primary(uint64_t now_us); /* stopwatch start/stop or pop a bubble */
void badge_tools_reset(uint64_t now_us);
void badge_tools_snapshot(uint64_t now_us, badge_tools_snapshot_t *out);
