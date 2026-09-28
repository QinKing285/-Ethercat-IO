/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CIA402_MOTOR_H
#define CIA402_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CIA402_CONTROLWORD_SHUTDOWN          (0x0006U)
#define CIA402_CONTROLWORD_SWITCH_ON         (0x0007U)
#define CIA402_CONTROLWORD_ENABLE_OPERATION  (0x000FU)
#define CIA402_CONTROLWORD_DISABLE_VOLTAGE   (0x0000U)
#define CIA402_CONTROLWORD_QUICK_STOP        (0x0002U)
#define CIA402_CONTROLWORD_FAULT_RESET       (0x0080U)

#define CIA402_MODE_CYCLIC_SYNC_VELOCITY     (9)

typedef enum {
    cia402_state_not_ready_to_switch_on = 0,
    cia402_state_switch_on_disabled,
    cia402_state_ready_to_switch_on,
    cia402_state_switched_on,
    cia402_state_operation_enabled,
    cia402_state_quick_stop_active,
    cia402_state_fault_reaction_active,
    cia402_state_fault,
} cia402_state_t;

void cia402_motor_init(bool foc_ready);
void cia402_motor_process(void);
void cia402_motor_shutdown(void);
void cia402_motor_write_controlword(uint16_t controlword);
uint16_t cia402_motor_get_controlword(void);
uint16_t cia402_motor_get_statusword(void);
uint16_t cia402_motor_get_error_code(void);
cia402_state_t cia402_motor_get_state(void);
const char *cia402_motor_get_state_name(void);
void cia402_motor_set_target_velocity(int32_t rpm);
int32_t cia402_motor_get_target_velocity(void);
int32_t cia402_motor_get_actual_velocity(void);
int8_t cia402_motor_get_mode(void);
bool cia402_motor_is_operation_enabled(void);

#ifdef __cplusplus
}
#endif

#endif
