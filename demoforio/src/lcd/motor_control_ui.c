/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "motor_control_ui.h"

#include "cia402_test_ui.h"
#include "ecat_ssc_service.h"
#include "lvgl.h"
#include "motor_foc_control.h"
#include "motor_monitor.h"

#define UI_DEFAULT_SPEED_RPM (600)
#define UI_SPEED_STEP_RPM    (100)
#define UI_CURRENT_CHART_PERIOD_MS       (20)
#define UI_CURRENT_CHART_POINT_COUNT     (80)
#define UI_CURRENT_CHART_DEFAULT_RANGE_INDEX (3U)

#define UI_COLOR_BACKGROUND  lv_color_hex(0x08111F)
#define UI_COLOR_PANEL       lv_color_hex(0x111C2E)
#define UI_COLOR_PANEL_LIGHT lv_color_hex(0x1D2A40)
#define UI_COLOR_TEXT        lv_color_hex(0xE8F0FC)
#define UI_COLOR_MUTED       lv_color_hex(0x8191A8)
#define UI_COLOR_ACCENT      lv_color_hex(0x22D3EE)
#define UI_COLOR_GREEN       lv_color_hex(0x16A34A)
#define UI_COLOR_RED         lv_color_hex(0xDC2626)
#define UI_COLOR_ORANGE      lv_color_hex(0xD97706)
#define UI_COLOR_MAGENTA     lv_color_hex(0xE879F9)

static lv_obj_t *speed_arc;
static lv_obj_t *target_value_label;
static lv_obj_t *actual_speed_label;
static lv_obj_t *bus_voltage_label;
static lv_obj_t *temperature_label;
static lv_obj_t *status_label;
static lv_obj_t *run_button;
static lv_obj_t *run_button_label;
static lv_obj_t *current_curve_view;
static lv_obj_t *current_chart;
static lv_obj_t *phase_a_value_label;
static lv_obj_t *phase_b_value_label;
static lv_obj_t *iq_reference_value_label;
static lv_obj_t *iq_actual_value_label;
static lv_obj_t *current_axis_top_label;
static lv_obj_t *current_axis_bottom_label;
static lv_obj_t *current_range_label;
static lv_obj_t *current_speed_label;
static lv_chart_series_t *phase_a_series;
static lv_chart_series_t *phase_b_series;
static lv_chart_series_t *iq_reference_series;
static lv_chart_series_t *iq_actual_series;
static lv_timer_t *speed_monitor_timer;
static lv_timer_t *current_curve_timer;
static bool ui_motor_ready;
static bool arc_is_updating;
static bool monitor_values_valid;
static uint32_t cached_bus_voltage_mv;
static int32_t cached_temperature_milli_c;
static uint32_t current_chart_range_index =
    UI_CURRENT_CHART_DEFAULT_RANGE_INDEX;

static const int32_t current_chart_ranges_deci_a[] = {
    10, 20, 50, 100, 200, 500
};

static void ui_set_status(const char *text, lv_color_t color)
{
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

static void ui_update_target_label(int32_t rpm)
{
    lv_label_set_text_fmt(target_value_label, "%ld", (long)rpm);
}

static void ui_set_target_speed(int32_t rpm)
{
    if (rpm < MOTOR_FOC_MIN_SPEED_RPM) {
        rpm = MOTOR_FOC_MIN_SPEED_RPM;
    } else if (rpm > MOTOR_FOC_MAX_SPEED_RPM) {
        rpm = MOTOR_FOC_MAX_SPEED_RPM;
    }

    motor_foc_set_target_rpm(rpm);
    ui_update_target_label(rpm);

    arc_is_updating = true;
    lv_arc_set_value(speed_arc, rpm);
    arc_is_updating = false;
}

static void ui_update_motor_buttons(void)
{
    bool foc_running = motor_foc_is_running();
    bool ecat_operational = ecat_ssc_service_is_operational();

    if (run_button != NULL && run_button_label != NULL) {
        lv_label_set_text(run_button_label,
                          ecat_operational ? "ECAT" :
                          (foc_running ? "STOP" : "START"));
        lv_obj_set_style_bg_color(run_button,
                                  ecat_operational ? UI_COLOR_ACCENT :
                                  (foc_running ? UI_COLOR_RED : UI_COLOR_GREEN),
                                  0);
        lv_obj_set_style_shadow_color(run_button,
                                      ecat_operational ? UI_COLOR_ACCENT :
                                      (foc_running ? UI_COLOR_RED : UI_COLOR_GREEN),
                                      0);
    }
}

static void speed_arc_event_cb(lv_event_t *event)
{
    int32_t rpm;

    if (arc_is_updating || lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    if (ecat_ssc_service_is_operational()) {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
        return;
    }

    rpm = lv_arc_get_value(lv_event_get_target_obj(event));
    ui_set_target_speed(rpm);
}

static void run_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    if (ecat_ssc_service_is_operational()) {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
        return;
    }

    if (motor_foc_is_running()) {
        motor_foc_stop();
        ui_set_status("STOPPED", UI_COLOR_ORANGE);
        ui_update_motor_buttons();
        return;
    }

    if (!ui_motor_ready || !motor_foc_start()) {
        ui_set_status("FAULT", UI_COLOR_RED);
        ui_update_motor_buttons();
        return;
    }
    ui_set_status("RUNNING", UI_COLOR_GREEN);
    ui_update_motor_buttons();
}

static void accelerate_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (ecat_ssc_service_is_operational()) {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
        return;
    }
    ui_set_target_speed(motor_foc_get_target_rpm() +
                        UI_SPEED_STEP_RPM);
}

static void decelerate_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (ecat_ssc_service_is_operational()) {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
        return;
    }
    ui_set_target_speed(motor_foc_get_target_rpm() -
                        UI_SPEED_STEP_RPM);
}

static int32_t ui_current_ma_to_deci_a(int32_t current_ma)
{
    return current_ma >= 0
               ? (current_ma + 50) / 100
               : (current_ma - 50) / 100;
}

static void ui_set_current_value_label(lv_obj_t *label, const char *name,
                                       int32_t current_ma)
{
    int32_t current_deci_a = current_ma >= 0
                                 ? (current_ma + 50) / 100
                                 : (current_ma - 50) / 100;
    uint32_t current_abs = (uint32_t)(current_deci_a < 0
                                          ? -current_deci_a
                                          : current_deci_a);

    lv_label_set_text_fmt(label, "%s %s%lu.%01luA", name,
                          current_deci_a < 0 ? "-" : "",
                          (unsigned long)(current_abs / 10U),
                          (unsigned long)(current_abs % 10U));
}

static void current_curve_timer_cb(lv_timer_t *timer)
{
    int32_t phase_a_ma;
    int32_t phase_b_ma;
    int32_t iq_reference_ma;
    int32_t iq_actual_ma;
    bool phase_current_valid;
    bool iq_current_valid;

    LV_UNUSED(timer);

    lv_label_set_text_fmt(
        current_speed_label, "%ldRPM",
        (long)(motor_foc_is_running() ? motor_foc_get_actual_rpm() : 0));

    phase_current_valid = motor_foc_get_phase_currents_ma(&phase_a_ma,
                                                          &phase_b_ma);
    iq_current_valid = motor_foc_get_iq_currents_ma(&iq_reference_ma,
                                                     &iq_actual_ma);

    if (phase_current_valid) {
        lv_chart_set_next_value(current_chart, phase_a_series,
                                ui_current_ma_to_deci_a(phase_a_ma));
        lv_chart_set_next_value(current_chart, phase_b_series,
                                ui_current_ma_to_deci_a(phase_b_ma));
        ui_set_current_value_label(phase_a_value_label, "IA", phase_a_ma);
        ui_set_current_value_label(phase_b_value_label, "IB", phase_b_ma);
    } else {
        lv_chart_set_next_value(current_chart, phase_a_series,
                                LV_CHART_POINT_NONE);
        lv_chart_set_next_value(current_chart, phase_b_series,
                                LV_CHART_POINT_NONE);
        lv_label_set_text(phase_a_value_label, "IA --.-A");
        lv_label_set_text(phase_b_value_label, "IB --.-A");
    }

    if (iq_current_valid) {
        lv_chart_set_next_value(current_chart, iq_reference_series,
                                ui_current_ma_to_deci_a(iq_reference_ma));
        lv_chart_set_next_value(current_chart, iq_actual_series,
                                ui_current_ma_to_deci_a(iq_actual_ma));
        ui_set_current_value_label(iq_reference_value_label, "Iq_ref",
                                   iq_reference_ma);
        ui_set_current_value_label(iq_actual_value_label, "Iq_actual",
                                   iq_actual_ma);
    } else {
        lv_chart_set_next_value(current_chart, iq_reference_series,
                                LV_CHART_POINT_NONE);
        lv_chart_set_next_value(current_chart, iq_actual_series,
                                LV_CHART_POINT_NONE);
        lv_label_set_text(iq_reference_value_label, "Iq_ref --.-A");
        lv_label_set_text(iq_actual_value_label, "Iq_actual --.-A");
    }
}

static void current_curve_delete_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DELETE) {
        return;
    }

    if (current_curve_timer != NULL) {
        lv_timer_delete(current_curve_timer);
        current_curve_timer = NULL;
    }

    current_curve_view = NULL;
    current_chart = NULL;
    phase_a_value_label = NULL;
    phase_b_value_label = NULL;
    iq_reference_value_label = NULL;
    iq_actual_value_label = NULL;
    current_axis_top_label = NULL;
    current_axis_bottom_label = NULL;
    current_range_label = NULL;
    current_speed_label = NULL;
    phase_a_series = NULL;
    phase_b_series = NULL;
    iq_reference_series = NULL;
    iq_actual_series = NULL;
}

static void current_curve_back_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        current_curve_view != NULL) {
        lv_obj_delete(current_curve_view);
    }
}

static lv_obj_t *ui_create_current_axis_label(lv_obj_t *parent,
                                              const char *text, int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_pos(label, 3, y);
    lv_obj_set_style_text_color(label, UI_COLOR_MUTED, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    return label;
}

static void ui_apply_current_chart_range(void)
{
    int32_t range_deci_a =
        current_chart_ranges_deci_a[current_chart_range_index];
    int32_t range_a = range_deci_a / 10;

    lv_chart_set_axis_range(current_chart, LV_CHART_AXIS_PRIMARY_Y,
                            -range_deci_a, range_deci_a);
    lv_label_set_text_fmt(current_axis_top_label, "+%ld", (long)range_a);
    lv_label_set_text_fmt(current_axis_bottom_label, "-%ld", (long)range_a);
    lv_label_set_text_fmt(current_range_label, "+/-%ldA", (long)range_a);
}

static void current_zoom_in_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        current_chart_range_index > 0U) {
        current_chart_range_index--;
        ui_apply_current_chart_range();
    }
}

static void current_zoom_out_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        current_chart_range_index + 1U <
            sizeof(current_chart_ranges_deci_a) /
                sizeof(current_chart_ranges_deci_a[0])) {
        current_chart_range_index++;
        ui_apply_current_chart_range();
    }
}

static lv_obj_t *ui_create_current_zoom_button(lv_obj_t *parent, int32_t x,
                                               const char *symbol,
                                               lv_event_cb_t event_cb)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, 3);
    lv_obj_set_size(button, 29, 28);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, UI_COLOR_PANEL_LIGHT, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(button);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_center(label);
    return button;
}

static void current_series_checkbox_event_cb(lv_event_t *event)
{
    lv_chart_series_t *series = lv_event_get_user_data(event);
    lv_obj_t *checkbox = lv_event_get_target_obj(event);

    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED &&
        current_chart != NULL && series != NULL) {
        lv_chart_hide_series(current_chart, series,
                             !lv_obj_has_state(checkbox,
                                               LV_STATE_CHECKED));
    }
}

static lv_obj_t *ui_create_current_series_checkbox(
    lv_obj_t *parent, int32_t x, int32_t width, const char *text,
    lv_color_t color, lv_chart_series_t *series)
{
    lv_obj_t *checkbox = lv_checkbox_create(parent);

    lv_obj_set_pos(checkbox, x, 210);
    lv_obj_set_size(checkbox, width, 30);
    lv_checkbox_set_text(checkbox, text);
    lv_obj_add_state(checkbox, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(checkbox, color, 0);
    lv_obj_set_style_text_font(checkbox, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_column(checkbox, 3, 0);
    lv_obj_set_style_size(checkbox, 14, 14, LV_PART_INDICATOR);
    lv_obj_set_style_radius(checkbox, 2, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(checkbox, 2, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(checkbox, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(checkbox, UI_COLOR_PANEL,
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(checkbox, color,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(checkbox, LV_OPA_COVER,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(checkbox, current_series_checkbox_event_cb,
                        LV_EVENT_VALUE_CHANGED, series);
    return checkbox;
}

static lv_obj_t *ui_create_current_value_label(
    lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
    const char *text, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    return label;
}

static void ui_create_current_curve_view(lv_obj_t *screen)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *back_label;

    if (current_curve_view != NULL) {
        return;
    }

    current_curve_view = lv_obj_create(screen);
    lv_obj_remove_style_all(current_curve_view);
    lv_obj_set_pos(current_curve_view, 0, 0);
    lv_obj_set_size(current_curve_view, 320, 240);
    lv_obj_set_style_bg_color(current_curve_view, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(current_curve_view, LV_OPA_COVER, 0);
    lv_obj_remove_flag(current_curve_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(current_curve_view, current_curve_delete_event_cb,
                        LV_EVENT_DELETE, NULL);

    header = lv_obj_create(current_curve_view);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 34);
    lv_obj_set_style_bg_color(header, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, UI_COLOR_PANEL_LIGHT, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 4, 3);
    lv_obj_set_size(back_button, 34, 28);
    lv_obj_set_style_radius(back_button, 8, 0);
    lv_obj_set_style_bg_color(back_button, UI_COLOR_PANEL_LIGHT, 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, current_curve_back_event_cb,
                        LV_EVENT_CLICKED, NULL);

    back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, UI_COLOR_TEXT, 0);
    lv_obj_center(back_label);

    ui_create_current_zoom_button(header, 42, LV_SYMBOL_MINUS,
                                  decelerate_button_event_cb);

    current_speed_label = lv_label_create(header);
    lv_label_set_text(current_speed_label, "0RPM");
    lv_obj_set_pos(current_speed_label, 74, 8);
    lv_obj_set_width(current_speed_label, 68);
    lv_obj_set_style_text_align(current_speed_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(current_speed_label, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(current_speed_label, &lv_font_montserrat_14, 0);

    ui_create_current_zoom_button(header, 145, LV_SYMBOL_PLUS,
                                  accelerate_button_event_cb);

    current_axis_top_label =
        ui_create_current_axis_label(current_curve_view, "+10", 34);
    ui_create_current_axis_label(current_curve_view, "0", 114);
    current_axis_bottom_label =
        ui_create_current_axis_label(current_curve_view, "-10", 192);

    current_chart = lv_chart_create(current_curve_view);
    lv_obj_set_pos(current_chart, 31, 36);
    lv_obj_set_size(current_chart, 281, 173);
    lv_chart_set_type(current_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(current_chart, UI_CURRENT_CHART_POINT_COUNT);
    lv_chart_set_update_mode(current_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(current_chart, 5, 8);
    lv_obj_set_style_radius(current_chart, 4, 0);
    lv_obj_set_style_bg_color(current_chart, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(current_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(current_chart, 1, 0);
    lv_obj_set_style_border_color(current_chart, UI_COLOR_PANEL_LIGHT, 0);
    lv_obj_set_style_line_color(current_chart, UI_COLOR_PANEL_LIGHT,
                                LV_PART_MAIN);
    lv_obj_set_style_line_width(current_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(current_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(current_chart, 0, 0, LV_PART_INDICATOR);

    phase_a_series = lv_chart_add_series(current_chart, UI_COLOR_ACCENT,
                                          LV_CHART_AXIS_PRIMARY_Y);
    phase_b_series = lv_chart_add_series(current_chart, UI_COLOR_ORANGE,
                                          LV_CHART_AXIS_PRIMARY_Y);
    iq_reference_series = lv_chart_add_series(current_chart, UI_COLOR_GREEN,
                                               LV_CHART_AXIS_PRIMARY_Y);
    iq_actual_series = lv_chart_add_series(current_chart, UI_COLOR_MAGENTA,
                                            LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(current_chart, phase_a_series, 0);
    lv_chart_set_all_values(current_chart, phase_b_series, 0);
    lv_chart_set_all_values(current_chart, iq_reference_series, 0);
    lv_chart_set_all_values(current_chart, iq_actual_series, 0);

    ui_create_current_series_checkbox(current_curve_view, 3, 55, "IA",
                                      UI_COLOR_ACCENT, phase_a_series);
    ui_create_current_series_checkbox(current_curve_view, 58, 55, "IB",
                                      UI_COLOR_ORANGE, phase_b_series);
    ui_create_current_series_checkbox(current_curve_view, 113, 89, "Iq_ref",
                                      UI_COLOR_GREEN, iq_reference_series);
    ui_create_current_series_checkbox(current_curve_view, 202, 118,
                                      "Iq_actual", UI_COLOR_MAGENTA,
                                      iq_actual_series);

    phase_a_value_label =
        ui_create_current_value_label(current_curve_view, 36, 39, 135,
                                      "IA 0.0A", UI_COLOR_ACCENT);
    phase_b_value_label =
        ui_create_current_value_label(current_curve_view, 174, 39, 135,
                                      "IB 0.0A", UI_COLOR_ORANGE);
    iq_reference_value_label =
        ui_create_current_value_label(current_curve_view, 36, 56, 135,
                                      "Iq_ref 0.0A", UI_COLOR_GREEN);
    iq_actual_value_label =
        ui_create_current_value_label(current_curve_view, 174, 56, 135,
                                      "Iq_actual 0.0A", UI_COLOR_MAGENTA);

    ui_create_current_zoom_button(header, 246, LV_SYMBOL_MINUS,
                                  current_zoom_out_event_cb);

    current_range_label = lv_label_create(header);
    lv_obj_set_pos(current_range_label, 177, 8);
    lv_obj_set_width(current_range_label, 65);
    lv_obj_set_style_text_align(current_range_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(current_range_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(current_range_label, &lv_font_montserrat_14, 0);

    ui_create_current_zoom_button(header, 283, LV_SYMBOL_PLUS,
                                  current_zoom_in_event_cb);
    ui_apply_current_chart_range();

    current_curve_timer = lv_timer_create(current_curve_timer_cb,
                                           UI_CURRENT_CHART_PERIOD_MS, NULL);
    current_curve_timer_cb(current_curve_timer);
}

static void current_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_create_current_curve_view(
            lv_obj_get_parent(lv_event_get_target_obj(event)));
    }
}

static void cia402_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        cia402_test_ui_open(
            lv_obj_get_parent(lv_event_get_target_obj(event)),
            ui_motor_ready);
        ui_update_motor_buttons();
    }
}

static void speed_monitor_timer_cb(lv_timer_t *timer)
{
    int32_t actual_rpm;
    int32_t temperature_milli_c;
    int32_t temperature_deci_c;
    uint32_t bus_voltage_mv;
    uint32_t bus_voltage_deci_v;
    uint32_t temperature_abs;

    LV_UNUSED(timer);

    if (motor_foc_control_has_fault()) {
        motor_foc_stop();
        ui_motor_ready = false;
        ui_set_status("FAULT", UI_COLOR_RED);
        ui_update_motor_buttons();
    } else if (ecat_ssc_service_is_operational()) {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
    }

    ui_update_motor_buttons();

    actual_rpm = motor_foc_is_running() ? motor_foc_get_actual_rpm() : 0;
    lv_label_set_text_fmt(actual_speed_label, "RPM %ld", (long)actual_rpm);

    ui_update_target_label(motor_foc_get_target_rpm());
    arc_is_updating = true;
    lv_arc_set_value(speed_arc, motor_foc_get_target_rpm());
    arc_is_updating = false;

    if (motor_monitor_get_values(&cached_bus_voltage_mv,
                                 &cached_temperature_milli_c)) {
        monitor_values_valid = true;
    }

    if (!monitor_values_valid) {
        lv_label_set_text(bus_voltage_label, "V --.-");
        lv_label_set_text(temperature_label, "T --.-C");
        return;
    }
    bus_voltage_mv = cached_bus_voltage_mv;
    temperature_milli_c = cached_temperature_milli_c;

    bus_voltage_deci_v = (bus_voltage_mv + 50U) / 100U;
    lv_label_set_text_fmt(bus_voltage_label, "V %lu.%01lu",
                          (unsigned long)(bus_voltage_deci_v / 10U),
                          (unsigned long)(bus_voltage_deci_v % 10U));

    temperature_deci_c = temperature_milli_c >= 0
                             ? (temperature_milli_c + 50) / 100
                             : (temperature_milli_c - 50) / 100;
    temperature_abs = (uint32_t)(temperature_deci_c < 0
                                     ? -temperature_deci_c
                                     : temperature_deci_c);
    lv_label_set_text_fmt(temperature_label, "T %s%lu.%01luC",
                          temperature_deci_c < 0 ? "-" : "",
                          (unsigned long)(temperature_abs / 10U),
                          (unsigned long)(temperature_abs % 10U));
}

static void motor_screen_delete_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DELETE) {
        return;
    }

    if (speed_monitor_timer != NULL) {
        lv_timer_delete(speed_monitor_timer);
        speed_monitor_timer = NULL;
    }

    if (!ecat_ssc_service_is_operational()) {
        motor_foc_stop();
    }
    speed_arc = NULL;
    target_value_label = NULL;
    actual_speed_label = NULL;
    bus_voltage_label = NULL;
    temperature_label = NULL;
    status_label = NULL;
    run_button = NULL;
    run_button_label = NULL;
    monitor_values_valid = false;
}

static lv_obj_t *ui_create_button(lv_obj_t *parent, int32_t y,
                                  const char *text, lv_color_t color,
                                  lv_event_cb_t event_cb, bool repeat)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(button, 194, y);
    lv_obj_set_size(button, 116, 31);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_color(button, color, 0);
    lv_obj_set_style_shadow_width(button, 8, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
    lv_obj_set_style_transform_scale_x(button, 245, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(button, 245, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, NULL);
    if (repeat) {
        lv_obj_add_event_cb(button, event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    }

    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return button;
}

void motor_control_ui_create(lv_obj_t *screen, bool motor_ready,
                             lv_event_cb_t back_event_cb)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *back_label;
    lv_obj_t *title_label;
    lv_obj_t *unit_label;
    lv_obj_t *range_label;
    lv_obj_t *range_max_label;

    ui_motor_ready = motor_ready;
    monitor_values_valid = false;
    lv_obj_add_event_cb(screen, motor_screen_delete_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, 320, 34);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, UI_COLOR_PANEL_LIGHT, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 4, 3);
    lv_obj_set_size(back_button, 34, 28);
    lv_obj_set_style_radius(back_button, 8, 0);
    lv_obj_set_style_bg_color(back_button, UI_COLOR_PANEL_LIGHT, 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_CLICKED, NULL);

    back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, UI_COLOR_TEXT, 0);
    lv_obj_center(back_label);

    title_label = lv_label_create(header);
    lv_label_set_text(title_label, "MOTOR FOC");
    lv_obj_set_pos(title_label, 47, 7);
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);

    status_label = lv_label_create(header);
    lv_obj_align(status_label, LV_ALIGN_RIGHT_MID, -11, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    ui_set_status(motor_ready ? "READY" : "FAULT",
                  motor_ready ? UI_COLOR_ACCENT : UI_COLOR_RED);

    speed_arc = lv_arc_create(screen);
    lv_obj_set_pos(speed_arc, 11, 43);
    lv_obj_set_size(speed_arc, 172, 172);
    lv_arc_set_rotation(speed_arc, 135);
    lv_arc_set_bg_angles(speed_arc, 0, 270);
    lv_arc_set_range(speed_arc, MOTOR_FOC_MIN_SPEED_RPM,
                     MOTOR_FOC_MAX_SPEED_RPM);
    lv_obj_set_style_arc_width(speed_arc, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_color(speed_arc, UI_COLOR_PANEL_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_arc_width(speed_arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(speed_arc, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(speed_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(speed_arc, UI_COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(speed_arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_color(speed_arc, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_border_width(speed_arc, 3, LV_PART_KNOB);
    lv_obj_set_style_pad_all(speed_arc, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(speed_arc, speed_arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    target_value_label = lv_label_create(screen);
    lv_obj_align_to(target_value_label, speed_arc, LV_ALIGN_CENTER, 0, -9);
    lv_obj_set_style_text_color(target_value_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(target_value_label, &lv_font_montserrat_24, 0);

    unit_label = lv_label_create(screen);
    lv_label_set_text(unit_label, "TARGET RPM");
    lv_obj_align_to(unit_label, speed_arc, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_text_color(unit_label, UI_COLOR_MUTED, 0);
    lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);

    range_label = lv_label_create(screen);
    lv_label_set_text(range_label, "0");
    lv_obj_set_pos(range_label, 27, 195);
    lv_obj_set_style_text_color(range_label, UI_COLOR_MUTED, 0);
    lv_obj_set_style_text_font(range_label, &lv_font_montserrat_14, 0);

    range_max_label = lv_label_create(screen);
    lv_label_set_text(range_max_label, "2400");
    lv_obj_set_pos(range_max_label, 143, 195);
    lv_obj_set_style_text_color(range_max_label, UI_COLOR_MUTED, 0);
    lv_obj_set_style_text_font(range_max_label, &lv_font_montserrat_14, 0);

    actual_speed_label = lv_label_create(screen);
    lv_label_set_text(actual_speed_label, "RPM 0");
    lv_obj_set_pos(actual_speed_label, 11, 218);
    lv_obj_set_style_text_color(actual_speed_label, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(actual_speed_label, &lv_font_montserrat_14, 0);

    bus_voltage_label = lv_label_create(screen);
    lv_label_set_text(bus_voltage_label, "V --.-");
    lv_obj_set_pos(bus_voltage_label, 122, 218);
    lv_obj_set_style_text_color(bus_voltage_label, UI_COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(bus_voltage_label, &lv_font_montserrat_14, 0);

    temperature_label = lv_label_create(screen);
    lv_label_set_text(temperature_label, "T --.-C");
    lv_obj_set_pos(temperature_label, 225, 218);
    lv_obj_set_style_text_color(temperature_label, UI_COLOR_GREEN, 0);
    lv_obj_set_style_text_font(temperature_label, &lv_font_montserrat_14, 0);

    run_button = ui_create_button(screen, 57, "START", UI_COLOR_GREEN,
                                  run_button_event_cb, false);
    run_button_label = lv_obj_get_child(run_button, 0);
    ui_create_button(screen, 109, "CURRENT", UI_COLOR_PANEL_LIGHT,
                     current_button_event_cb, false);
    ui_create_button(screen, 161, "CIA402 TEST", UI_COLOR_ORANGE,
                     cia402_button_event_cb, false);

    if (!ecat_ssc_service_is_operational()) {
        ui_set_target_speed(UI_DEFAULT_SPEED_RPM);
    } else {
        ui_set_status("ECAT OP", UI_COLOR_ACCENT);
    }
    ui_update_motor_buttons();
    speed_monitor_timer = lv_timer_create(speed_monitor_timer_cb, 200, NULL);
    speed_monitor_timer_cb(speed_monitor_timer);
}
