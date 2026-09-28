/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ecat_ssc_service.h"

#include <stdio.h>

#include "applInterface.h"
#include "bldc_foc.h"
#include "board.h"
#include "cia402appl.h"
#include "ecat_def.h"
#include "ecatappl.h"
#include "ecatslv.h"
#include "hpm_ecat_hw.h"

#define ECAT_AL_STATE_MASK  (0x0FU)
#define ECAT_AL_STATE_INIT  (0x01U)
#define ECAT_AL_STATE_PREOP (0x02U)
#define ECAT_AL_STATE_SAFEOP (0x04U)
#define ECAT_AL_STATE_OP    (0x08U)

#define ECAT_CIA402_ERROR_FOC_NOT_READY (0xFF01U)
#define ECAT_CIA402_ERROR_FOC_FAULT     (0xFF02U)

extern TCiA402Axis LocalAxes[MAX_AXES];

static bool service_ready;
static uint8_t previous_al_state = ECAT_AL_STATE_INIT;
static bool foc_fault_reported;

static TCiA402Axis *ecat_axis(void)
{
    return service_ready ? &LocalAxes[0] : NULL;
}

bool ecat_ssc_service_init(void)
{
    hpm_stat_t stat;
    uint16_t result;

    if (service_ready) {
        return true;
    }

    board_init_ethercat(HPM_ESC);
    stat = ecat_hardware_init(HPM_ESC);
    if (stat != status_success) {
        printf("ECAT: ESC hardware initialization failed (%d)\r\n", stat);
        return false;
    }

    board_delay_ms(1000);
    motor_function_init();

    result = MainInit();
    if (result != 0U) {
        printf("ECAT: SSC MainInit failed (0x%04X)\r\n", result);
        return false;
    }

#if defined(ESC_EEPROM_EMULATION) && ESC_EEPROM_EMULATION
    pAPPL_EEPROM_Read = ecat_eeprom_emulation_read;
    pAPPL_EEPROM_Write = ecat_eeprom_emulation_write;
    pAPPL_EEPROM_Reload = ecat_eeprom_emulation_reload;
    pAPPL_EEPROM_Store = ecat_eeprom_emulation_store;
#endif

    result = CiA402_Init();
    if (result != 0U) {
        printf("ECAT: CiA402 initialization failed (0x%04X)\r\n", result);
        return false;
    }

    result = APPL_GenerateMapping(&nPdInputSize, &nPdOutputSize);
    if (result != 0U) {
        printf("ECAT: PDO mapping failed (0x%04X)\r\n", result);
        CiA402_DeallocateAxis();
        return false;
    }

    /* The shared GUI motor_foc currently supports cyclic velocity control. */
    LocalAxes[0].Objects.objSupportedDriveModes = 0x100U;
    LocalAxes[0].Objects.objModesOfOperation = CYCLIC_SYNC_VELOCITY_MODE;
    LocalAxes[0].Objects.objModesOfOperationDisplay =
        CYCLIC_SYNC_VELOCITY_MODE;

    service_ready = true;
    previous_al_state = nAlStatus & ECAT_AL_STATE_MASK;
    bRunApplication = TRUE;

    if (!ecat_motor_adapter_is_ready()) {
        CiA402_LocalError(ECAT_CIA402_ERROR_FOC_NOT_READY);
        foc_fault_reported = true;
    }

    printf("ECAT: SSC CiA402 stack ready, CSV mode\r\n");
    return true;
}

void ecat_ssc_service_task(void)
{
    uint8_t al_state;

    if (!service_ready || bRunApplication != TRUE) {
        return;
    }

    MainLoop();
    al_state = nAlStatus & ECAT_AL_STATE_MASK;

    LocalAxes[0].Objects.objSupportedDriveModes = 0x100U;

    if (previous_al_state == ECAT_AL_STATE_OP &&
        al_state != ECAT_AL_STATE_OP) {
        ecat_motor_adapter_force_stop();
    }
    previous_al_state = al_state;

    if (!ecat_motor_adapter_is_ready() && !foc_fault_reported) {
        CiA402_LocalError(ECAT_CIA402_ERROR_FOC_FAULT);
        ecat_motor_adapter_force_stop();
        foc_fault_reported = true;
    }
}

bool ecat_ssc_service_is_ready(void)
{
    return service_ready;
}

bool ecat_ssc_service_motor_is_ready(void)
{
    return ecat_motor_adapter_is_ready();
}

bool ecat_ssc_service_is_operational(void)
{
    return service_ready &&
           ((nAlStatus & ECAT_AL_STATE_MASK) == ECAT_AL_STATE_OP);
}

bool ecat_ssc_service_dc_is_active(void)
{
    return service_ready && (bDcSyncActive == TRUE);
}

uint8_t ecat_ssc_service_get_al_state(void)
{
    return service_ready ? (nAlStatus & ECAT_AL_STATE_MASK) : 0U;
}

const char *ecat_ssc_service_get_al_state_name(void)
{
    if (!service_ready) {
        return "ERROR";
    }

    switch (ecat_ssc_service_get_al_state()) {
    case ECAT_AL_STATE_INIT:
        return "INIT";
    case ECAT_AL_STATE_PREOP:
        return "PREOP";
    case ECAT_AL_STATE_SAFEOP:
        return "SAFEOP";
    case ECAT_AL_STATE_OP:
        return "OP";
    default:
        return "UNKNOWN";
    }
}

uint16_t ecat_ssc_service_get_controlword(void)
{
    TCiA402Axis *axis = ecat_axis();
    return axis != NULL ? axis->Objects.objControlWord : 0U;
}

uint16_t ecat_ssc_service_get_statusword(void)
{
    TCiA402Axis *axis = ecat_axis();
    return axis != NULL ? axis->Objects.objStatusWord : 0U;
}

uint16_t ecat_ssc_service_get_error_code(void)
{
    TCiA402Axis *axis = ecat_axis();
    return axis != NULL ? axis->Objects.objErrorCode : 0U;
}

int16_t ecat_ssc_service_get_mode(void)
{
    TCiA402Axis *axis = ecat_axis();
    return axis != NULL ? axis->Objects.objModesOfOperationDisplay : 0;
}

int32_t ecat_ssc_service_get_target_velocity(void)
{
    TCiA402Axis *axis = ecat_axis();
    return axis != NULL ? axis->Objects.objTargetVelocity : 0;
}

int32_t ecat_ssc_service_get_actual_velocity(void)
{
    TCiA402Axis *axis = ecat_axis();

    if (axis == NULL) {
        return 0;
    }
    axis->Objects.objVelocityActualValue = motor_get_actual_speed();
    return axis->Objects.objVelocityActualValue;
}

void ecat_ssc_service_set_controlword(uint16_t controlword)
{
    TCiA402Axis *axis = ecat_axis();
    if (axis != NULL) {
        axis->Objects.objControlWord = controlword;
    }
}

void ecat_ssc_service_set_target_velocity(int32_t velocity)
{
    TCiA402Axis *axis = ecat_axis();
    if (axis != NULL) {
        axis->Objects.objTargetVelocity = velocity;
    }
}
