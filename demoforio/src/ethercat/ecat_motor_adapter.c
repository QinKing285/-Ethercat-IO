/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "bldc_foc.h"

#include <limits.h>

#include "motor_foc_control.h"

/* Keep the velocity scaling used by the SDK ecat_cia402 motor example. */
#define CIA402_SPEED_TO_RPM_NUMERATOR    (65532LL)
#define CIA402_SPEED_TO_RPM_DENOMINATOR  (10000LL)

static bool adapter_ready;

mcl_user_value_t motor_speed;
mcl_user_value_t motor_position;

static int32_t clamp_i64_to_i32(int64_t value)
{
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

int32_t ecat_motor_velocity_to_rpm(int32_t velocity)
{
    int64_t scaled = (int64_t)velocity * CIA402_SPEED_TO_RPM_NUMERATOR;

    if (scaled >= 0) {
        scaled += CIA402_SPEED_TO_RPM_DENOMINATOR / 2;
    } else {
        scaled -= CIA402_SPEED_TO_RPM_DENOMINATOR / 2;
    }
    return clamp_i64_to_i32(scaled / CIA402_SPEED_TO_RPM_DENOMINATOR);
}

int32_t ecat_motor_rpm_to_velocity(int32_t rpm)
{
    int64_t scaled = (int64_t)rpm * CIA402_SPEED_TO_RPM_DENOMINATOR;

    if (scaled >= 0) {
        scaled += CIA402_SPEED_TO_RPM_NUMERATOR / 2;
    } else {
        scaled -= CIA402_SPEED_TO_RPM_NUMERATOR / 2;
    }
    return clamp_i64_to_i32(scaled / CIA402_SPEED_TO_RPM_NUMERATOR);
}

void motor_function_init(void)
{
    adapter_ready = motor_foc_control_init();
    motor_speed.enable = true;
    motor_speed.value = 0.0f;
    motor_position.enable = false;
    motor_position.value = 0.0f;
    motor_foc_stop();
}

void motor_speed_loop_init(void)
{
    motor_speed.enable = true;
    motor_position.enable = false;
    motor_foc_set_target_rpm(0);
}

void motor_speed_loop_set(int32_t target_speed)
{
    int32_t target_rpm = ecat_motor_velocity_to_rpm(target_speed);

    motor_speed.value = (float)target_speed;
    motor_foc_set_target_rpm(target_rpm);
}

void motor_postion_loop_init(void)
{
    /* The shared GUI FOC currently exposes CSV speed control only. */
    motor_speed.enable = false;
    motor_position.enable = true;
}

void motor_position_loop_set(int32_t target_position)
{
    (void)target_position;
}

int32_t motor_get_actual_speed(void)
{
    return ecat_motor_rpm_to_velocity(motor_foc_get_actual_rpm());
}

int32_t motor_get_actual_position(void)
{
    return 0;
}

void motor_enable(void)
{
    if (!adapter_ready || motor_foc_control_has_fault()) {
        return;
    }

    (void)motor_foc_start();
}

void motor_disable(void)
{
    motor_foc_stop();
}

bool ecat_motor_adapter_is_ready(void)
{
    return adapter_ready && !motor_foc_control_has_fault();
}

void ecat_motor_adapter_force_stop(void)
{
    motor_disable();
}
