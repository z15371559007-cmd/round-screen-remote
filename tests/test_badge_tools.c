#include <assert.h>
#include <stdio.h>
#include "badge_tools.h"

int main(void)
{
    badge_tools_snapshot_t s;
    badge_tools_enter();
    badge_tools_snapshot(0, &s);
    assert(s.page == BADGE_TOOL_STOPWATCH && !s.running && s.elapsed_ms == 0);

    badge_tools_primary(1000000); /* start */
    badge_tools_snapshot(4650123ULL, &s);
    assert(s.running && s.elapsed_ms == 3650);
    badge_tools_primary(5000000); /* stop */
    badge_tools_snapshot(5500000, &s);
    assert(!s.running && s.elapsed_ms == 4000);

    badge_tools_primary(6000000); /* start, then exit pauses */
    badge_tools_exit(8100123);
    badge_tools_snapshot(9000000, &s);
    assert(!s.running && s.elapsed_ms == 6100);

    badge_tools_reset(10000000);
    badge_tools_next_page();
    badge_tools_primary(11000000);
    badge_tools_primary(12000000);
    badge_tools_snapshot(13000000, &s);
    assert(s.page == BADGE_TOOL_BUBBLE && s.bubbles_popped == 2);
    badge_tools_reset(14000000);
    badge_tools_snapshot(15000000, &s);
    assert(s.bubbles_popped == 0);

    badge_tools_next_page();
    badge_tools_reset(15000000);
    badge_tools_primary(16000000);
    badge_tools_snapshot(17000000, &s);
    assert(s.page == BADGE_TOOL_STOPWATCH && s.elapsed_ms == 1000);
    puts("badge tools state tests passed");
    return 0;
}
