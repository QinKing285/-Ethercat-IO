/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "motor_monitor.h"

#include "board.h"
#include "hpm_adc16_drv.h"

#define MOTOR_MONITOR_TEMP_ADC_BASE            HPM_ADC1
#define MOTOR_MONITOR_TEMP_ADC_CHANNEL         (10U)
#define MOTOR_MONITOR_VBUS_ADC_BASE            HPM_ADC3
#define MOTOR_MONITOR_VBUS_ADC_CHANNEL         (7U)
#define MOTOR_MONITOR_ADC_REFERENCE_UV         (3300000UL)
#define MOTOR_MONITOR_ADC_FULL_SCALE           (65535UL)
#define MOTOR_MONITOR_SAMPLE_COUNT             (4U)
#define MOTOR_MONITOR_VBUS_TOP_RESISTOR_OHM    (200000UL)
#define MOTOR_MONITOR_VBUS_BOTTOM_RESISTOR_OHM (10000UL)
#define MOTOR_MONITOR_MCP9700_OFFSET_UV         (500000L)
#define MOTOR_MONITOR_MCP9700_SLOPE_UV_PER_C   (10000L)

static bool motor_monitor_initialized;

static void motor_monitor_init_pins(void)
{
    HPM_IOC->PAD[IOC_PAD_PF08].FUNC_CTL = IOC_PAD_FUNC_CTL_ANALOG_MASK;
    HPM_IOC->PAD[IOC_PAD_PF31].FUNC_CTL = IOC_PAD_FUNC_CTL_ANALOG_MASK;
}

static bool motor_monitor_init_temperature_adc(void)
{
    adc16_config_t adc_config;
    adc16_channel_config_t channel_config;

    board_init_adc_clock(MOTOR_MONITOR_TEMP_ADC_BASE, true);
    adc16_get_default_config(&adc_config);
    adc_config.res = adc16_res_16_bits;
    adc_config.adc_clk_div = adc16_clock_divider_4;
    adc_config.wait_dis = false;
    adc_config.sel_sync_ahb = false;
    adc_config.adc_ahb_en = true;
    if (adc16_init(MOTOR_MONITOR_TEMP_ADC_BASE, &adc_config) !=
        status_success) {
        return false;
    }

    adc16_get_channel_default_config(&channel_config);
    channel_config.sample_cycle = 20;
    channel_config.ch = MOTOR_MONITOR_TEMP_ADC_CHANNEL;
    if (adc16_init_channel(MOTOR_MONITOR_TEMP_ADC_BASE, &channel_config) !=
        status_success) {
        return false;
    }
    return true;
}

bool motor_monitor_init(void)
{
    adc16_channel_config_t channel_config;

    motor_monitor_initialized = false;
    motor_monitor_init_pins();

    if (!motor_monitor_init_temperature_adc()) {
        return false;
    }

    /* ADC3 is already configured by FOC; only add the VBUS channel. */
    adc16_get_channel_default_config(&channel_config);
    channel_config.sample_cycle = 20;
    channel_config.ch = MOTOR_MONITOR_VBUS_ADC_CHANNEL;
    if (adc16_init_channel(MOTOR_MONITOR_VBUS_ADC_BASE, &channel_config) !=
        status_success) {
        return false;
    }

    adc16_set_blocking_read(MOTOR_MONITOR_TEMP_ADC_BASE);
    adc16_set_blocking_read(MOTOR_MONITOR_VBUS_ADC_BASE);
#if defined(ADC_SOC_BUSMODE_ENABLE_CTRL_SUPPORT) && ADC_SOC_BUSMODE_ENABLE_CTRL_SUPPORT
    adc16_enable_oneshot_mode(MOTOR_MONITOR_TEMP_ADC_BASE);
    adc16_enable_oneshot_mode(MOTOR_MONITOR_VBUS_ADC_BASE);
#endif
    motor_monitor_initialized = true;
    return true;
}

static bool motor_monitor_read_average(ADC16_Type *adc, uint8_t channel,
                                       uint16_t *average)
{
    uint32_t sum = 0;
    uint16_t sample;

    for (uint32_t i = 0; i < MOTOR_MONITOR_SAMPLE_COUNT; i++) {
        if (adc16_get_oneshot_result(adc, channel, &sample) != status_success) {
            return false;
        }
        sum += sample;
    }

    *average = (uint16_t)((sum + (MOTOR_MONITOR_SAMPLE_COUNT / 2U)) /
                         MOTOR_MONITOR_SAMPLE_COUNT);
    return true;
}

bool motor_monitor_get_values(uint32_t *bus_voltage_mv,
                              int32_t *temperature_milli_c)
{
    uint16_t bus_adc;
    uint16_t temperature_adc;
    uint64_t bus_numerator;
    uint64_t bus_denominator;
    int32_t temperature_uv;

    if (!motor_monitor_initialized || bus_voltage_mv == NULL ||
        temperature_milli_c == NULL) {
        return false;
    }

    if (!motor_monitor_read_average(MOTOR_MONITOR_VBUS_ADC_BASE,
                                    MOTOR_MONITOR_VBUS_ADC_CHANNEL, &bus_adc) ||
        !motor_monitor_read_average(MOTOR_MONITOR_TEMP_ADC_BASE,
                                    MOTOR_MONITOR_TEMP_ADC_CHANNEL,
                                    &temperature_adc)) {
        return false;
    }

    bus_numerator = (uint64_t)bus_adc * MOTOR_MONITOR_ADC_REFERENCE_UV *
                    (MOTOR_MONITOR_VBUS_TOP_RESISTOR_OHM +
                     MOTOR_MONITOR_VBUS_BOTTOM_RESISTOR_OHM);
    bus_denominator = (uint64_t)MOTOR_MONITOR_ADC_FULL_SCALE *
                      MOTOR_MONITOR_VBUS_BOTTOM_RESISTOR_OHM * 1000U;
    *bus_voltage_mv = (uint32_t)((bus_numerator + (bus_denominator / 2U)) /
                                 bus_denominator);

    temperature_uv =
        (int32_t)(((uint64_t)temperature_adc *
                   MOTOR_MONITOR_ADC_REFERENCE_UV +
                   (MOTOR_MONITOR_ADC_FULL_SCALE / 2U)) /
                  MOTOR_MONITOR_ADC_FULL_SCALE);
    *temperature_milli_c =
        (temperature_uv - MOTOR_MONITOR_MCP9700_OFFSET_UV) * 1000L /
        MOTOR_MONITOR_MCP9700_SLOPE_UV_PER_C;
    return true;
}
