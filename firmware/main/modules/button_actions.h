#pragma once

#include "hw_buttons.h"

/*
 * The only place that maps physical keys to actions. Change key behaviour here only.
 *   Upper key (PWR):  in PTT mode, hold to talk; in every other mode, tap = play/pause.
 *   Lower key (BOOT): tap = back to home menu; long press = power off.
 */
void button_actions_handle(hw_button_event_t event);
