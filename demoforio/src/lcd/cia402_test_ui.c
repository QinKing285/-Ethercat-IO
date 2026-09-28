/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cia402_test_ui.h"

#include <stdint.h>

#include "cia402_motor.h"
#include "ecat_ssc_service.h"

#define CIA402_UI_PERIOD_MS       (50U)
#define CIA402_UI_SPEED_STEP_RPM  (100)

#define CIA402_COLOR_BACKGROUND   lv_color_hex(0x08111F)
#define CIA402_COLOR_PANEL        lv_color_hex(0x111C2E)
#define CIA402_COLOR_PANEL_LIGHT  lv_color_hex(0x1D2A40)
#define CIA402_COLOR_TEXT         lv_color_hex(0xE8F0FC)
#define CIA402_COLOR_MUTED        lv_color_hex(0x8191A8)
#define CIA402_COLOR_ACCENT       lv_color_hex(0x22D3EE)
#define CIA402_COLOR_GREEN        lv_color_hex(0x16A34A)
#define CIA402_COLOR_ORANGE       lv_color_hex(0xD97706)
#define CIA402_COLOR_RED          lv_color_hex(0xDC2626)
#define CIA402_COLOR_BLUE         lv_color_hex(0x2563EB)

static lv_obj_t *cia402_view;
static lv_obj_t *cia402_state_label;
static lv_obj_t *cia402_object_label;
static lv_obj_t *cia402_target_label;
static lv_obj_t *cia402_actual_label;
static lv_timer_t *cia402_timer;

static lv_color_t cia402_state_color(cia402_state_t state)
{
    switch (state) {
    case cia402_state_operation_enabled:
        return CIA402_COLOR_GREEN;
    case cia402_state_fault:
    case cia402_state_fault_reaction_active:
        return CIA402_COLOR_RED;
    case cia402_state_ready_to_switch_on:
    case cia402_state_switched_on:
    case cia402_state_quick_stop_active:
        return CIA402_COLOR_ORANGE;
    default:
        return CIA402_COLOR_ACCENT;
    }
}

static void cia402_update_view(void)
{
    cia402_state_t state;

    if (cia402_view == NULL) {
        return;
    }

    cia402_motor_process();
    state = cia402_motor_get_state();
    lv_label_set_text_fmt(cia402_state_label, "ECAT %s %s | %s",
                          ecat_ssc_service_get_al_state_name(),
                          ecat_ssc_service_dc_is_active() ? "DC" : "FREE",
                          cia402_motor_get_state_name());
    lv_obj_set_style_text_color(cia402_state_label,
                                cia402_state_color(state), 0);
    lv_label_set_text_fmt(cia402_object_label,
                          "6040:%04X  6041:%04X  603F:%04X",
                          cia402_motor_get_controlword(),
                          cia402_motor_get_statusword(),
                          cia402_motor_get_error_code());
    lv_label_set_text_fmt(cia402_target_label, "60FF TARGET %ld RPM",
                          (long)cia402_motor_get_target_velocity());
    lv_label_set_text_fmt(cia402_actual_label, "606C ACTUAL %ld RPM",
                          (long)cia402_motor_get_actual_velocity());
}

static void cia402_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    cia402_update_view();
}

static void cia402_command_event_cb(lv_event_t *event)
{
    uint16_t controlword;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (ecat_ssc_service_is_operational()) {
        cia402_update_view();
        return;
    }

    controlword = (uint16_t)(uintptr_t)lv_event_get_user_data(event);
    cia402_motor_write_controlword(controlword);
    cia402_update_view();
}

static void cia402_target_decrease_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED ||
        lv_event_get_code(event) == LV_EVENT_LONG_PRESSED_REPEAT) {
        if (ecat_ssc_service_is_operational()) {
            cia402_update_view();
            return;
        }
        cia402_motor_set_target_velocity(
            cia402_motor_get_target_velocity() -
            CIA402_UI_SPEED_STEP_RPM);
        cia402_update_view();
    }
}

static void cia402_target_increase_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED ||
        lv_event_get_code(event) == LV_EVENT_LONG_PRESSED_REPEAT) {
        if (ecat_ssc_service_is_operational()) {
            cia402_update_view();
            return;
        }
        cia402_motor_set_target_velocity(
            cia402_motor_get_target_velocity() +
            CIA402_UI_SPEED_STEP_RPM);
        cia402_update_view();
    }
}

static void cia402_view_delete_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DELETE) {
        return;
    }

    if (cia402_timer != NULL) {
        lv_timer_delete(cia402_timer);
        cia402_timer = NULL;
    }
    cia402_view = NULL;
    cia402_state_label = NULL;
    cia402_object_label = NULL;
    cia402_target_label = NULL;
    cia402_actual_label = NULL;
}

static void cia402_back_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        cia402_view != NULL) {
        lv_obj_delete(cia402_view);
    }
}

static lv_obj_t *cia402_create_button(lv_obj_t *parent, int32_t x,
                                      int32_t y, int32_t width,
                                      const char *text, lv_color_t color,
                                      lv_event_cb_t callback,
                                      void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, 31);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *cia402_create_step_button(lv_obj_t *parent, int32_t x,
                                           const char *symbol,
                                           lv_event_cb_t callback)
{
    lv_obj_t *button = cia402_create_button(
        parent, x, 178, 42, symbol, CIA402_COLOR_PANEL_LIGHT, callback, NULL);

    lv_obj_add_event_cb(button, callback, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    return button;
}

void cia402_test_ui_open(lv_obj_t *parent, bool motor_ready)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *back_label;
    lv_obj_t *title_label;
    lv_obj_t *mode_label;

    if (cia402_view != NULL) {
        return;
    }

    LV_UNUSED(motor_ready);
    cia402_motor_init(motor_ready);

    cia402_view = lv_obj_create(parent);
    lv_obj_remove_style_all(cia402_view);
    lv_obj_set_pos(cia402_view, 0, 0);
    lv_obj_set_size(cia402_view, 320, 240);
    lv_obj_set_style_bg_color(cia402_view, CIA402_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(cia402_view, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cia402_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(cia402_view, cia402_view_delete_event_cb,
                        LV_EVENT_DELETE, NULL);

    header = lv_obj_create(cia402_view);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 34);
    lv_obj_set_style_bg_color(header, CIA402_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, CIA402_COLOR_PANEL_LIGHT, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 4, 3);
    lv_obj_set_size(back_button, 34, 28);
    lv_obj_set_style_radius(back_button, 6, 0);
    lv_obj_set_style_bg_color(back_button, CIA402_COLOR_PANEL_LIGHT, 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, cia402_back_event_cb,
                        LV_EVENT_CLICKED, NULL);

    back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, CIA402_COLOR_TEXT, 0);
    lv_obj_center(back_label);

    title_label = lv_label_create(header);
    lv_label_set_text(title_label, "ECAT CIA402");
    lv_obj_set_pos(title_label, 47, 7);
    lv_obj_set_style_text_color(title_label, CIA402_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);

    mode_label = lv_label_create(header);
    lv_label_set_text_fmt(mode_label, "CSV %d", cia402_motor_get_mode());
    lv_obj_align(mode_label, LV_ALIGN_RIGHT_MID, -9, 0);
    lv_obj_set_style_text_color(mode_label, CIA402_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_14, 0);

    cia402_state_label = lv_label_create(cia402_view);
    lv_obj_set_pos(cia402_state_label, 8, 40);
    lv_obj_set_width(cia402_state_label, 304);
    lv_obj_set_style_text_align(cia402_state_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cia402_state_label,
                               &lv_font_montserrat_14, 0);

    cia402_object_label = lv_label_create(cia402_view);
    lv_obj_set_pos(cia402_object_label, 4, 61);
    lv_obj_set_width(cia402_object_label, 312);
    lv_obj_set_style_text_align(cia402_object_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(cia402_object_label, CIA402_COLOR_MUTED, 0);
    lv_obj_set_style_text_font(cia402_object_label,
                               &lv_font_montserrat_14, 0);

    cia402_create_button(cia402_view, 5, 83, 100, "SHUTDOWN",
                         CIA402_COLOR_BLUE, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_SHUTDOWN);
    cia402_create_button(cia402_view, 110, 83, 100, "SWITCH ON",
                         CIA402_COLOR_BLUE, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_SWITCH_ON);
    cia402_create_button(cia402_view, 215, 83, 100, "ENABLE OP",
                         CIA402_COLOR_GREEN, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_ENABLE_OPERATION);
    cia402_create_button(cia402_view, 5, 119, 100, "QUICK STOP",
                         CIA402_COLOR_ORANGE, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_QUICK_STOP);
    cia402_create_button(cia402_view, 110, 119, 100, "DISABLE",
                         CIA402_COLOR_ORANGE, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_DISABLE_VOLTAGE);
    cia402_create_button(cia402_view, 215, 119, 100, "FAULT RESET",
                         CIA402_COLOR_RED, cia402_command_event_cb,
                         (void *)(uintptr_t)CIA402_CONTROLWORD_FAULT_RESET);

    cia402_target_label = lv_label_create(cia402_view);
    lv_obj_set_pos(cia402_target_label, 50, 157);
    lv_obj_set_width(cia402_target_label, 220);
    lv_obj_set_style_text_align(cia402_target_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(cia402_target_label, CIA402_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(cia402_target_label,
                               &lv_font_montserrat_14, 0);

    cia402_create_step_button(cia402_view, 76, LV_SYMBOL_MINUS,
                              cia402_target_decrease_event_cb);
    cia402_create_step_button(cia402_view, 202, LV_SYMBOL_PLUS,
                              cia402_target_increase_event_cb);

    cia402_actual_label = lv_label_create(cia402_view);
    lv_obj_set_pos(cia402_actual_label, 50, 215);
    lv_obj_set_width(cia402_actual_label, 220);
    lv_obj_set_style_text_align(cia402_actual_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(cia402_actual_label, CIA402_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(cia402_actual_label,
                               &lv_font_montserrat_14, 0);

    cia402_timer = lv_timer_create(cia402_timer_cb,
                                   CIA402_UI_PERIOD_MS, NULL);
    cia402_update_view();
}
