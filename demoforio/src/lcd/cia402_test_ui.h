/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CIA402_TEST_UI_H
#define CIA402_TEST_UI_H

#include <stdbool.h>

#include "lvgl.h"

void cia402_test_ui_open(lv_obj_t *parent, bool motor_ready);

#endif
