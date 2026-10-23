#!/usr/bin/env python3
"""Regression guard: Badge entry must not do blocking setup in the LVGL callback."""
from pathlib import Path
import re

source = Path("firmware/main/modules/badge.c").read_text()
board = Path("firmware/main/board/board_port_waveshare.c").read_text()
header = Path("firmware/main/modules/badge.h").read_text()

def body(signature: str, text: str) -> str:
    match = re.search(re.escape(signature) + r"\s*\{", text)
    assert match, f"missing function {signature}"
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if depth == 0:
            return text[start:i]
    raise AssertionError(f"unterminated function {signature}")

entry = body("esp_err_t badge_enter(void)", source)
assert "init_once(" not in entry, "Badge entry must not allocate/scan synchronously"
assert "start_wifi(" not in entry, "Badge entry must not start Wi-Fi synchronously"
assert "xTaskCreatePinnedToCore" in entry, "Badge entry must hand setup off to a worker"
worker = body("static void player_task(void *arg)", source)
assert worker.index("init_once()") < worker.index("start_wifi()"), "worker should initialize before Wi-Fi"
assert "BADGE_STATE_STARTING" in header and "吧唧启动中" in board, "startup must be represented in UI"
print("badge entry nonblocking regression check passed")
