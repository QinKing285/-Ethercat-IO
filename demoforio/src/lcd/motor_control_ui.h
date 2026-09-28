/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOTOR_CONTROL_UI_H
#define MOTOR_CONTROL_UI_H

#include <stdbool.h>
#include "lvgl.h"

void motor_control_ui_create(lv_obj_t *screen, bool motor_ready,
                             lv_event_cb_t back_event_cb);

#endif /* MOTOR_CONTROL_UI_H */
