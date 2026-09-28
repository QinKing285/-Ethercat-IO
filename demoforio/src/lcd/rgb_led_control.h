/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RGB_LED_CONTROL_H
#define RGB_LED_CONTROL_H

#include <stdint.h>

typedef enum {
    rgb_led_color_red = 0,
    rgb_led_color_green,
    rgb_led_color_blue,
    rgb_led_color_count
} rgb_led_color_t;

void rgb_led_control_init(void);
void rgb_led_control_set(rgb_led_color_t color, uint8_t brightness_percent);
void rgb_led_control_off(void);

#endif
