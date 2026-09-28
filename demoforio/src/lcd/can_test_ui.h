/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CAN_TEST_UI_H
#define CAN_TEST_UI_H

#include "lvgl.h"

void can_test_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb);
void can_test_ui_stop(void);

#endif
