/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ECAT_MOTOR_ADAPTER_H
#define ECAT_MOTOR_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "hpm_mcl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Motor hooks expected by the SDK SSC CiA402 application. */
void motor_function_init(void);
void motor_speed_loop_init(void);
void motor_speed_loop_set(int32_t target_speed);
void motor_postion_loop_init(void);
void motor_position_loop_set(int32_t target_position);
int32_t motor_get_actual_speed(void);
int32_t motor_get_actual_position(void);
void motor_enable(void);
void motor_disable(void);

extern mcl_user_value_t motor_speed;
extern mcl_user_value_t motor_position;

bool ecat_motor_adapter_is_ready(void);
void ecat_motor_adapter_force_stop(void);
int32_t ecat_motor_velocity_to_rpm(int32_t velocity);
int32_t ecat_motor_rpm_to_velocity(int32_t rpm);

#ifdef __cplusplus
}
#endif

#endif /* ECAT_MOTOR_ADAPTER_H */
