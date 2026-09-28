/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UART_TEST_UI_H
#define UART_TEST_UI_H

#include "lvgl.h"

void uart_test_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb);
void uart_test_ui_stop(void);

#endif
