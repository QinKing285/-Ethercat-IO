/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD_APP_H
#define LCD_APP_H

#include "type.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void lcd_app_hardware_init(void);
void lcd_app_ui_init(void);
void lcd_app_task_handler(void);
u8 KeyGetValue(u8 key);
void delay_ms(u32 ms);
void delay_us(u32 ms);

#ifdef __cplusplus
}
#endif

#endif
