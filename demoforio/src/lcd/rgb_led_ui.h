/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RGB_LED_UI_H
#define RGB_LED_UI_H

#include "lvgl.h"

void rgb_led_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb);
void rgb_led_ui_stop(void);

#endif
