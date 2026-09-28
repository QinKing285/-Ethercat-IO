/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rgb_led_ui.h"

#include <stdbool.h>
#include <stdint.h>

#include "rgb_led_control.h"

#define RGB_UI_BACKGROUND lv_color_hex(0xF3F6FA)
#define RGB_UI_SURFACE    lv_color_hex(0xFFFFFF)
#define RGB_UI_BORDER     lv_color_hex(0xD6DEE8)
#define RGB_UI_TEXT       lv_color_hex(0x172033)

typedef enum {
    rgb_mode_red = 0,
    rgb_mode_green,
    rgb_mode_blue,
    rgb_mode_breathe,
    rgb_mode_count
} rgb_mode_t;

static const uint32_t rgb_colors[rgb_led_color_count] = {
    [rgb_led_color_red] = 0xEF4444,
    [rgb_led_color_green] = 0x22C55E,
    [rgb_led_color_blue] = 0x3B82F6,
};

static const char *const mode_names[rgb_mode_count] = {
    [rgb_mode_red] = "RED",
    [rgb_mode_green] = "GREEN",
    [rgb_mode_blue] = "BLUE",
    [rgb_mode_breathe] = "BREATHE",
};

static lv_timer_t *breathe_timer;
static lv_obj_t *preview;
static lv_obj_t *mode_buttons[rgb_mode_count];
static rgb_led_color_t breathe_color;
static uint8_t breathe_level;
static bool breathe_increasing;

static void update_preview(rgb_led_color_t color, uint8_t brightness)
{
    if (preview == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(preview, lv_color_hex(rgb_colors[color]), 0);
    lv_obj_set_style_bg_opa(preview,
                            (lv_opa_t)(LV_OPA_20 +
                                       ((uint32_t)(LV_OPA_COVER - LV_OPA_20) *
                                        brightness) / 100U),
                            0);
}

static void set_selected_mode(rgb_mode_t selected)
{
    uint32_t index;

    for (index = 0; index < rgb_mode_count; index++) {
        bool active = index == (uint32_t)selected;

        lv_obj_set_style_border_width(mode_buttons[index], active ? 2 : 1, 0);
        lv_obj_set_style_border_color(
            mode_buttons[index],
            active ? lv_color_hex(index < rgb_mode_breathe ? rgb_colors[index]
                                                            : 0x06B6D4)
                   : RGB_UI_BORDER,
            0);
    }
}

static void stop_breathe_timer(void)
{
    if (breathe_timer != NULL) {
        lv_timer_delete(breathe_timer);
        breathe_timer = NULL;
    }
}

static void breathe_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (breathe_increasing) {
        if (breathe_level >= 98U) {
            breathe_level = 100U;
            breathe_increasing = false;
        } else {
            breathe_level += 2U;
        }
    } else if (breathe_level <= 2U) {
        breathe_level = 0U;
        breathe_increasing = true;
        breathe_color = (rgb_led_color_t)((breathe_color + 1U) %
                                           rgb_led_color_count);
    } else {
        breathe_level -= 2U;
    }

    rgb_led_control_set(breathe_color, breathe_level);
    update_preview(breathe_color, breathe_level);
}

static void mode_button_event_cb(lv_event_t *event)
{
    rgb_mode_t mode;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    mode = (rgb_mode_t)(uintptr_t)lv_event_get_user_data(event);
    stop_breathe_timer();
    set_selected_mode(mode);

    if (mode < rgb_mode_breathe) {
        rgb_led_control_set((rgb_led_color_t)mode, 100U);
        update_preview((rgb_led_color_t)mode, 100U);
        return;
    }

    breathe_color = rgb_led_color_red;
    breathe_level = 0U;
    breathe_increasing = true;
    rgb_led_control_set(breathe_color, breathe_level);
    update_preview(breathe_color, breathe_level);
    breathe_timer = lv_timer_create(breathe_timer_cb, 20, NULL);
}

static void create_mode_button(lv_obj_t *screen, rgb_mode_t mode,
                               int32_t x, int32_t y)
{
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_t *label;
    lv_color_t accent = lv_color_hex(mode < rgb_mode_breathe
                                         ? rgb_colors[mode]
                                         : 0x06B6D4);

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 148, 37);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, RGB_UI_SURFACE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xE8EDF3),
                              LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, RGB_UI_BORDER, 0);
    lv_obj_set_style_shadow_width(button, 3, 0);
    lv_obj_set_style_shadow_offset_y(button, 1, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_10, 0);
    lv_obj_add_event_cb(button, mode_button_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)mode);

    label = lv_label_create(button);
    lv_label_set_text(label, mode_names[mode]);
    lv_obj_set_style_text_color(label, mode < rgb_mode_breathe ? accent
                                                               : RGB_UI_TEXT,
                                0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);

    mode_buttons[mode] = button;
}

void rgb_led_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *label;

    rgb_led_control_init();
    rgb_led_control_off();
    lv_obj_set_style_bg_color(screen, RGB_UI_BACKGROUND, 0);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 38);
    lv_obj_set_style_bg_color(header, RGB_UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, RGB_UI_BORDER, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 5, 4);
    lv_obj_set_size(back_button, 34, 30);
    lv_obj_set_style_radius(back_button, 6, 0);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xE8EDF3), 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(back_button);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, RGB_UI_TEXT, 0);
    lv_obj_center(label);

    label = lv_label_create(header);
    lv_label_set_text(label, "RGB LED");
    lv_obj_set_style_text_color(label, RGB_UI_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(label, 49, 8);

    preview = lv_obj_create(screen);
    lv_obj_remove_style_all(preview);
    lv_obj_set_size(preview, 94, 94);
    lv_obj_set_style_radius(preview, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(preview, 5, 0);
    lv_obj_set_style_border_color(preview, RGB_UI_SURFACE, 0);
    lv_obj_set_style_shadow_color(preview, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_shadow_width(preview, 10, 0);
    lv_obj_set_style_shadow_opa(preview, LV_OPA_20, 0);
    lv_obj_align(preview, LV_ALIGN_TOP_MID, 0, 48);
    update_preview(rgb_led_color_red, 0U);

    create_mode_button(screen, rgb_mode_red, 8, 149);
    create_mode_button(screen, rgb_mode_green, 164, 149);
    create_mode_button(screen, rgb_mode_blue, 8, 193);
    create_mode_button(screen, rgb_mode_breathe, 164, 193);
}

void rgb_led_ui_stop(void)
{
    uint32_t index;

    stop_breathe_timer();
    rgb_led_control_off();
    preview = NULL;
    for (index = 0; index < rgb_mode_count; index++) {
        mode_buttons[index] = NULL;
    }
}
