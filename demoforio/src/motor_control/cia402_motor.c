/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cia402_motor.h"

#include "bldc_foc.h"
#include "ecat_ssc_service.h"
#include "motor_foc_control.h"

#define CIA402_STATUSWORD_STATE_MASK        (0x006FU)
#define CIA402_STATUS_NOT_READY             (0x0000U)
#define CIA402_STATUS_SWITCH_ON_DISABLED    (0x0040U)
#define CIA402_STATUS_READY_TO_SWITCH_ON    (0x0021U)
#define CIA402_STATUS_SWITCHED_ON           (0x0023U)
#define CIA402_STATUS_OPERATION_ENABLED     (0x0027U)
#define CIA402_STATUS_QUICK_STOP_ACTIVE     (0x0007U)
#define CIA402_STATUS_FAULT_REACTION_ACTIVE (0x000FU)
#define CIA402_STATUS_FAULT                 (0x0008U)

void cia402_motor_init(bool foc_ready)
{
    (void)foc_ready;
}

void cia402_motor_process(void)
{
    /* The SSC MainLoop owns the CiA402 state machine. */
}

void cia402_motor_shutdown(void)
{
    /* Closing the GUI must not interrupt an EtherCAT controlled axis. */
}

void cia402_motor_write_controlword(uint16_t controlword)
{
    ecat_ssc_service_set_controlword(controlword);
}

uint16_t cia402_motor_get_controlword(void)
{
    return ecat_ssc_service_get_controlword();
}

uint16_t cia402_motor_get_statusword(void)
{
    return ecat_ssc_service_get_statusword();
}

uint16_t cia402_motor_get_error_code(void)
{
    return ecat_ssc_service_get_error_code();
}

cia402_state_t cia402_motor_get_state(void)
{
    switch (cia402_motor_get_statusword() & CIA402_STATUSWORD_STATE_MASK) {
    case CIA402_STATUS_SWITCH_ON_DISABLED:
        return cia402_state_switch_on_disabled;
    case CIA402_STATUS_READY_TO_SWITCH_ON:
        return cia402_state_ready_to_switch_on;
    case CIA402_STATUS_SWITCHED_ON:
        return cia402_state_switched_on;
    case CIA402_STATUS_OPERATION_ENABLED:
        return cia402_state_operation_enabled;
    case CIA402_STATUS_QUICK_STOP_ACTIVE:
        return cia402_state_quick_stop_active;
    case CIA402_STATUS_FAULT_REACTION_ACTIVE:
        return cia402_state_fault_reaction_active;
    case CIA402_STATUS_FAULT:
        return cia402_state_fault;
    case CIA402_STATUS_NOT_READY:
    default:
        return cia402_state_not_ready_to_switch_on;
    }
}

const char *cia402_motor_get_state_name(void)
{
    static const char *const names[] = {
        "NOT READY",
        "SWITCH ON DISABLED",
        "READY TO SWITCH ON",
        "SWITCHED ON",
        "OPERATION ENABLED",
        "QUICK STOP ACTIVE",
        "FAULT REACTION",
        "FAULT",
    };
    cia402_state_t state = cia402_motor_get_state();

    if ((uint32_t)state >= (sizeof(names) / sizeof(names[0]))) {
        return "UNKNOWN";
    }
    return names[state];
}

void cia402_motor_set_target_velocity(int32_t rpm)
{
    if (rpm < MOTOR_FOC_MIN_SPEED_RPM) {
        rpm = MOTOR_FOC_MIN_SPEED_RPM;
    } else if (rpm > MOTOR_FOC_MAX_SPEED_RPM) {
        rpm = MOTOR_FOC_MAX_SPEED_RPM;
    }

    ecat_ssc_service_set_target_velocity(ecat_motor_rpm_to_velocity(rpm));
}

int32_t cia402_motor_get_target_velocity(void)
{
    return ecat_motor_velocity_to_rpm(
        ecat_ssc_service_get_target_velocity());
}

int32_t cia402_motor_get_actual_velocity(void)
{
    return ecat_motor_velocity_to_rpm(
        ecat_ssc_service_get_actual_velocity());
}

int8_t cia402_motor_get_mode(void)
{
    return (int8_t)ecat_ssc_service_get_mode();
}

bool cia402_motor_is_operation_enabled(void)
{
    return cia402_motor_get_state() == cia402_state_operation_enabled;
}
