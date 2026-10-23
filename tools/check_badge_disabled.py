#!/usr/bin/env python3
"""Regression guard: the unstable video-player page is not reachable or compiled."""
from pathlib import Path
import re

read = lambda path: Path(path).read_text()
app_mode = read("firmware/main/app_mode.h")
router = read("firmware/main/mode_router.c")
board = read("firmware/main/board/board_port_waveshare.c")
board_api = read("firmware/main/board/board_port.h")
component = read("firmware/main/CMakeLists.txt")
partitions = read("firmware/partitions.csv")

assert not re.search(r"\bAPP_MODE_BADGE\b", app_mode), "video-player mode must be absent"
assert not re.search(r"\bAPP_MODE_BADGE\b|badge_enter|badge_exit|#include \"badge\.h\"", router), "router must not expose video player"
assert not re.search(r"\bAPP_MODE_BADGE\b|s_badge_img|board_port_badge_show_frame|吧唧", board), "screen must not render or navigate to video player"
assert "board_port_badge_show_frame" not in board_api, "video frame renderer must be disconnected"
assert '"modules/badge.c"' not in component and "web/upload.html" not in component, "video player and upload page must not be compiled/embedded"
assert '"modules/badge_tools.c"' in component and "APP_MODE_BADGE_TOOLS" in app_mode, "the independent stopwatch/bubble/key-ring tools must remain"
for active_mode in ("APP_MODE_MEDIA_REMOTE", "APP_MODE_PUSH_TO_TALK", "APP_MODE_STANDBY_PET"):
    assert active_mode in app_mode and active_mode in router and active_mode in board, f"unrelated mode removed: {active_mode}"
assert "badge,    data, 0x40,    0x410000, 0x800000" in partitions, "keep the existing partition table unchanged"
print("video-player disabled; existing tools, modes, and partition layout preserved")
