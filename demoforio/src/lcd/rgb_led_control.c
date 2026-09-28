/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rgb_led_control.h"

#include <stdbool.h>

#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_pwmv2_drv.h"

#define RGB_PWM_PERIOD_MS (1U)

typedef struct {
    PWMV2_Type *pwm;
    clock_name_t clock_name;
    pwm_channel_t channel;
    uint32_t reload;
    uint8_t shadow;
    bool cmp_initial_zero;
} rgb_pwm_t;

static rgb_pwm_t rgb_pwm[rgb_led_color_count] = {
    [rgb_led_color_red] = {
        .pwm = BOARD_RED_PWM,
        .clock_name = BOARD_RED_PWM_CLOCK_NAME,
        .channel = BOARD_RED_PWM_OUT,
        .shadow = 0,
        .cmp_initial_zero = BOARD_RED_PWM_CMP_INITIAL_ZERO,
    },
    [rgb_led_color_green] = {
        .pwm = BOARD_GREEN_PWM,
        .clock_name = BOARD_GREEN_PWM_CLOCK_NAME,
        .channel = BOARD_GREEN_PWM_OUT,
        .shadow = 3,
        .cmp_initial_zero = BOARD_GREEN_PWM_CMP_INITIAL_ZERO,
    },
    [rgb_led_color_blue] = {
        .pwm = BOARD_BLUE_PWM,
        .clock_name = BOARD_BLUE_PWM_CLOCK_NAME,
        .channel = BOARD_BLUE_PWM_OUT,
        .shadow = 6,
        .cmp_initial_zero = BOARD_BLUE_PWM_CMP_INITIAL_ZERO,
    },
};

static bool rgb_initialized;

static void configure_pwm(rgb_pwm_t *led)
{
    pwm_counter_t counter = led->channel / 2;
    uint8_t cmp_1 = led->channel * 2;
    uint8_t cmp_2 = cmp_1 + 1;
    uint8_t cmp_1_shadow = led->shadow + 1;
    uint8_t cmp_2_shadow = led->shadow + 2;
    uint32_t cmp_1_initial = led->cmp_initial_zero ? 0U : led->reload;

    pwmv2_disable_shadow_lock_feature(led->pwm);
    pwmv2_disable_counter(led->pwm, counter);
    pwmv2_reset_counter(led->pwm, counter);

    pwmv2_set_shadow_val(led->pwm, led->shadow, led->reload, 0, false);
    pwmv2_set_shadow_val(led->pwm, cmp_1_shadow, cmp_1_initial, 0, false);
    pwmv2_set_shadow_val(led->pwm, cmp_2_shadow, led->reload, 0, false);

    pwmv2_counter_select_data_offset_from_shadow_value(led->pwm, counter,
                                                       led->shadow);
    pwmv2_set_reload_update_time(led->pwm, counter, pwm_reload_update_on_shlk);
    pwmv2_select_cmp_source(led->pwm, cmp_1, cmp_value_from_shadow_val,
                            cmp_1_shadow);
    pwmv2_cmp_update_trig_time(led->pwm, cmp_1,
                               pwm_shadow_register_update_on_shlk);
    pwmv2_select_cmp_source(led->pwm, cmp_2, cmp_value_from_shadow_val,
                            cmp_2_shadow);
    pwmv2_cmp_update_trig_time(led->pwm, cmp_2,
                               pwm_shadow_register_update_on_shlk);
    pwmv2_issue_shadow_register_lock_event(led->pwm);

    pwmv2_counter_burst_disable(led->pwm, counter);
    pwmv2_disable_four_cmp(led->pwm, HPM_NUM_TO_EVEN_FLOOR(led->channel));
    if (led->cmp_initial_zero && !board_get_led_pwm_off_level()) {
        pwmv2_enable_output_invert(led->pwm, led->channel);
    }
    pwmv2_cmp_update_trig_time(led->pwm, cmp_1,
                               pwm_shadow_register_update_on_reload);
}

static void set_brightness(rgb_led_color_t color, uint8_t brightness_percent)
{
    rgb_pwm_t *led = &rgb_pwm[color];
    uint32_t compare;

    if (brightness_percent > 100U) {
        brightness_percent = 100U;
    }

    compare = (led->reload * brightness_percent) / 100U;
    if (!led->cmp_initial_zero) {
        compare = led->reload - compare;
    }
    pwmv2_set_shadow_val(led->pwm, led->shadow + 1, compare, 0, false);
}

void rgb_led_control_init(void)
{
    uint32_t index;

    if (rgb_initialized) {
        return;
    }

    board_init_rgb_pwm_pins();
    for (index = 0; index < rgb_led_color_count; index++) {
        rgb_pwm[index].reload =
            clock_get_frequency(rgb_pwm[index].clock_name) /
            1000U * RGB_PWM_PERIOD_MS - 1U;
        board_disable_output_rgb_led((uint8_t)index);
        configure_pwm(&rgb_pwm[index]);
    }

    /* Red and green share PWM3 counter 0; starting it twice is harmless. */
    for (index = 0; index < rgb_led_color_count; index++) {
        pwm_counter_t counter = rgb_pwm[index].channel / 2;

        pwmv2_enable_counter(rgb_pwm[index].pwm, counter);
        pwmv2_start_pwm_output(rgb_pwm[index].pwm, counter);
    }
    rgb_initialized = true;
}

void rgb_led_control_set(rgb_led_color_t color, uint8_t brightness_percent)
{
    uint32_t index;

    if (color >= rgb_led_color_count) {
        return;
    }
    rgb_led_control_init();

    for (index = 0; index < rgb_led_color_count; index++) {
        board_disable_output_rgb_led((uint8_t)index);
        set_brightness((rgb_led_color_t)index, 0U);
    }

    set_brightness(color, brightness_percent);
    if (brightness_percent > 0U) {
        board_enable_output_rgb_led((uint8_t)color);
    }
}

void rgb_led_control_off(void)
{
    uint32_t index;

    if (!rgb_initialized) {
        return;
    }
    for (index = 0; index < rgb_led_color_count; index++) {
        set_brightness((rgb_led_color_t)index, 0U);
        board_disable_output_rgb_led((uint8_t)index);
    }
}
