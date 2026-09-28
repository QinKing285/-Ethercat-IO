/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOTOR_FOC_CONTROL_H
#define MOTOR_FOC_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_FOC_MIN_SPEED_RPM (0)
#define MOTOR_FOC_MAX_SPEED_RPM (2400)

bool motor_foc_control_init(void);
bool motor_foc_start(void);
void motor_foc_stop(void);
void motor_foc_set_target_rpm(int32_t rpm);
int32_t motor_foc_get_target_rpm(void);
int32_t motor_foc_get_actual_rpm(void);
bool motor_foc_get_phase_currents_ma(int32_t *phase_a_ma,
                                     int32_t *phase_b_ma);
bool motor_foc_get_iq_currents_ma(int32_t *reference_ma,
                                  int32_t *actual_ma);
bool motor_foc_is_running(void);
bool motor_foc_control_has_fault(void);

#ifdef __cplusplus
}
#endif

#endif
