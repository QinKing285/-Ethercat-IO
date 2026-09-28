/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ECAT_SSC_SERVICE_H
#define ECAT_SSC_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ecat_ssc_service_init(void);
void ecat_ssc_service_task(void);
bool ecat_ssc_service_is_ready(void);
bool ecat_ssc_service_motor_is_ready(void);
bool ecat_ssc_service_is_operational(void);
bool ecat_ssc_service_dc_is_active(void);
uint8_t ecat_ssc_service_get_al_state(void);
const char *ecat_ssc_service_get_al_state_name(void);

uint16_t ecat_ssc_service_get_controlword(void);
uint16_t ecat_ssc_service_get_statusword(void);
uint16_t ecat_ssc_service_get_error_code(void);
int16_t ecat_ssc_service_get_mode(void);
int32_t ecat_ssc_service_get_target_velocity(void);
int32_t ecat_ssc_service_get_actual_velocity(void);
void ecat_ssc_service_set_controlword(uint16_t controlword);
void ecat_ssc_service_set_target_velocity(int32_t velocity);

#ifdef __cplusplus
}
#endif

#endif /* ECAT_SSC_SERVICE_H */
