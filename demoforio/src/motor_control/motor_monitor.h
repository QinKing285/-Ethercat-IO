/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOTOR_MONITOR_H
#define MOTOR_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool motor_monitor_init(void);
bool motor_monitor_get_values(uint32_t *bus_voltage_mv,
                              int32_t *temperature_milli_c);

#ifdef __cplusplus
}
#endif

#endif
