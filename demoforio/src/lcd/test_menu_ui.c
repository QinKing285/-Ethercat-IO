/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "test_menu_ui.h"

#include <stdbool.h>
#include <stdint.h>

#include "type.h"
#include "lvgl.h"
#include "LCDTest.h"
#include "button_test_ui.h"
#include "can_test_ui.h"
#include "ecat_ssc_service.h"
#include "motor_control_ui.h"
#include "motor_foc_control.h"
#include "motor_monitor.h"
#include "rgb_led_ui.h"
#include "uart_test_ui.h"

#define UI_COLOR_BACKGROUND  lv_color_hex(0x08111F)
#define UI_COLOR_PANEL       lv_color_hex(0x111C2E)
#define UI_COLOR_PANEL_LIGHT lv_color_hex(0x1D2A40)
#define UI_COLOR_TEXT        lv_color_hex(0xE8F0FC)
#define UI_COLOR_MUTED       lv_color_hex(0x8191A8)
#define UI_COLOR_ACCENT      lv_color_hex(0x22D3EE)
#define UI_COLOR_GREEN       lv_color_hex(0x22C55E)
#define UI_MAIN_BACKGROUND   lv_color_hex(0xF3F6FA)
#define UI_MAIN_SURFACE      lv_color_hex(0xFFFFFF)
#define UI_MAIN_BORDER       lv_color_hex(0xD6DEE8)
#define UI_MAIN_TEXT         lv_color_hex(0x172033)

typedef enum {
    test_item_rgb_led = 0,
    test_item_button,
    test_item_uart,
    test_item_ethernet,
    test_item_can,
    test_item_motor,
    test_item_touch_calibration,
    test_item_count
} test_item_id_t;

typedef struct {
    const char *title;
    const char *description;
    uint32_t color;
} test_item_t;

static const test_item_t test_items[test_item_count] = {
    [test_item_rgb_led] = {
        .title = "RGB LED",
        .description = "RGB LED hardware test",
        .color = 0xEF4444,
    },
    [test_item_button] = {
        .title = "BUTTON",
        .description = "Board button input test",
        .color = 0xF59E0B,
    },
    [test_item_uart] = {
        .title = "UART",
        .description = "Serial send and receive test",
        .color = 0x3B82F6,
    },
    [test_item_ethernet] = {
        .title = "ETHERNET",
        .description = "Ethernet link and data test",
        .color = 0x14B8A6,
    },
    [test_item_can] = {
        .title = "CAN",
        .description = "CAN send and receive test",
        .color = 0x8B5CF6,
    },
    [test_item_motor] = {
        .title = "MOTOR",
        .description = "BLDC FOC motor test",
        .color = 0x22C55E,
    },
    [test_item_touch_calibration] = {
        .title = "TOUCH CAL",
        .description = "Four-point touch calibration",
        .color = 0x06B6D4,
    },
};

static bool motor_init_attempted;
static bool motor_ready;

static void show_main_menu(bool back_animation);

static lv_obj_t *create_base_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

static void load_screen(lv_obj_t *screen, bool back_animation)
{
    lv_screen_load_anim(screen,
                        back_animation ? LV_SCR_LOAD_ANIM_MOVE_RIGHT
                                       : LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, true);
}

static void back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        show_main_menu(true);
    }
}

static lv_obj_t *create_header(lv_obj_t *screen, const char *title, bool show_back)
{
    lv_obj_t *header;
    lv_obj_t *title_label;

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 38);
    lv_obj_set_style_bg_color(header, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, UI_COLOR_PANEL_LIGHT, 0);

    if (show_back) {
        lv_obj_t *button = lv_button_create(header);
        lv_obj_t *label;

        lv_obj_set_pos(button, 5, 4);
        lv_obj_set_size(button, 34, 30);
        lv_obj_set_style_radius(button, 8, 0);
        lv_obj_set_style_bg_color(button, UI_COLOR_PANEL_LIGHT, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_add_event_cb(button, back_button_event_cb, LV_EVENT_CLICKED, NULL);

        label = lv_label_create(button);
        lv_label_set_text(label, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
        lv_obj_center(label);
    }

    title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, show_back ? 49 : 11, 8);
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    return header;
}

static void show_placeholder_screen(test_item_id_t item_id)
{
    const test_item_t *item = &test_items[item_id];
    lv_obj_t *screen = create_base_screen();
    lv_obj_t *status_box;
    lv_obj_t *status_label;
    lv_obj_t *description_label;
    lv_obj_t *hint_label;

    create_header(screen, item->title, true);

    status_box = lv_obj_create(screen);
    lv_obj_set_pos(status_box, 37, 62);
    lv_obj_set_size(status_box, 246, 104);
    lv_obj_set_style_radius(status_box, 16, 0);
    lv_obj_set_style_bg_color(status_box, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(status_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_box, 2, 0);
    lv_obj_set_style_border_color(status_box, lv_color_hex(item->color), 0);
    lv_obj_set_style_shadow_color(status_box, lv_color_hex(item->color), 0);
    lv_obj_set_style_shadow_width(status_box, 14, 0);
    lv_obj_set_style_shadow_opa(status_box, LV_OPA_20, 0);

    status_label = lv_label_create(status_box);
    lv_label_set_text(status_label, "FRAMEWORK READY");
    lv_obj_set_style_text_color(status_label, lv_color_hex(item->color), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 11);

    description_label = lv_label_create(status_box);
    lv_label_set_text(description_label, item->description);
    lv_obj_set_width(description_label, 218);
    lv_obj_set_style_text_align(description_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(description_label, UI_COLOR_TEXT, 0);
    lv_obj_align(description_label, LV_ALIGN_CENTER, 0, 11);

    hint_label = lv_label_create(screen);
    lv_label_set_text(hint_label, "Hardware example will be integrated here later.");
    lv_obj_set_width(hint_label, 292);
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint_label, UI_COLOR_MUTED, 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -31);

    load_screen(screen, false);
}

static void motor_back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        if (!ecat_ssc_service_is_operational()) {
            motor_foc_stop();
        }
        show_main_menu(true);
    }
}

static void show_motor_screen(void)
{
    lv_obj_t *screen = create_base_screen();

    motor_control_ui_create(screen, motor_ready, motor_back_button_event_cb);
    load_screen(screen, false);
}

static void motor_init_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    motor_init_attempted = true;
    motor_ready = motor_foc_control_init();
    if (motor_ready) {
        motor_monitor_init();
    }
    show_motor_screen();
}

static void show_motor_loading_screen(void)
{
    lv_obj_t *screen;
    lv_obj_t *spinner;
    lv_obj_t *label;
    lv_timer_t *timer;

    if (motor_init_attempted) {
        show_motor_screen();
        return;
    }

    screen = create_base_screen();
    create_header(screen, "MOTOR TEST", false);

    spinner = lv_spinner_create(screen);
    lv_obj_set_size(spinner, 66, 66);
    lv_obj_set_style_arc_width(spinner, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, UI_COLOR_PANEL_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, UI_COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -18);

    label = lv_label_create(screen);
    lv_label_set_text(label, "Initializing FOC and aligning rotor...");
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 37);

    load_screen(screen, false);

    timer = lv_timer_create(motor_init_timer_cb, 300, NULL);
    lv_timer_set_repeat_count(timer, 1);
}

static void rgb_led_back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        rgb_led_ui_stop();
        show_main_menu(true);
    }
}

static void show_rgb_led_screen(void)
{
    lv_obj_t *screen = create_base_screen();

    rgb_led_ui_create(screen, rgb_led_back_button_event_cb);
    load_screen(screen, false);
}

static void button_test_back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        button_test_ui_stop();
        show_main_menu(true);
    }
}

static void show_button_test_screen(void)
{
    lv_obj_t *screen = create_base_screen();

    button_test_ui_create(screen, button_test_back_button_event_cb);
    load_screen(screen, false);
}

static void uart_test_back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        uart_test_ui_stop();
        show_main_menu(true);
    }
}

static void show_uart_test_screen(void)
{
    lv_obj_t *screen = create_base_screen();

    uart_test_ui_create(screen, uart_test_back_button_event_cb);
    load_screen(screen, false);
}

static void can_test_back_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        can_test_ui_stop();
        show_main_menu(true);
    }
}

static void show_can_test_screen(void)
{
    lv_obj_t *screen = create_base_screen();

    can_test_ui_create(screen, can_test_back_button_event_cb);
    load_screen(screen, false);
}

static void test_item_event_cb(lv_event_t *event)
{
    test_item_id_t item_id;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    item_id = (test_item_id_t)(uintptr_t)lv_event_get_user_data(event);
    if (item_id == test_item_rgb_led) {
        show_rgb_led_screen();
    } else if (item_id == test_item_button) {
        show_button_test_screen();
    } else if (item_id == test_item_uart) {
        show_uart_test_screen();
    } else if (item_id == test_item_can) {
        show_can_test_screen();
    } else if (item_id == test_item_motor) {
        show_motor_loading_screen();
    } else if (item_id == test_item_touch_calibration) {
        Touch_Test();
        lv_obj_invalidate(lv_screen_active());
    } else {
        show_placeholder_screen(item_id);
    }
}

static void create_test_item_button(lv_obj_t *screen, test_item_id_t item_id,
                                    int32_t x, int32_t y)
{
    const test_item_t *item = &test_items[item_id];
    lv_color_t color = lv_color_hex(item->color);
    lv_obj_t *button;
    lv_obj_t *title_label;
    lv_obj_t *accent_bar;

    button = lv_button_create(screen);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 148, 42);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, UI_MAIN_SURFACE, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xE8EDF3),
                              LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, UI_MAIN_BORDER, 0);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_shadow_width(button, 4, 0);
    lv_obj_set_style_shadow_offset_y(button, 1, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_10, 0);
    lv_obj_add_event_cb(button, test_item_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)item_id);

    accent_bar = lv_obj_create(button);
    lv_obj_remove_style_all(accent_bar);
    lv_obj_set_size(accent_bar, 4, 24);
    lv_obj_set_style_radius(accent_bar, 2, 0);
    lv_obj_set_style_bg_color(accent_bar, color, 0);
    lv_obj_set_style_bg_opa(accent_bar, LV_OPA_COVER, 0);
    lv_obj_align(accent_bar, LV_ALIGN_LEFT_MID, 7, 0);

    title_label = lv_label_create(button);
    lv_label_set_text(title_label, item->title);
    lv_obj_set_style_text_color(title_label, UI_MAIN_TEXT, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 19, 0);
}

static void show_main_menu(bool back_animation)
{
    lv_obj_t *screen = create_base_screen();
    lv_obj_t *header;
    lv_obj_t *title_label;
    uint32_t item_index;

    lv_obj_set_style_bg_color(screen, UI_MAIN_BACKGROUND, 0);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 36);
    lv_obj_set_style_bg_color(header, UI_MAIN_SURFACE, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, UI_MAIN_BORDER, 0);

    title_label = lv_label_create(header);
    lv_label_set_text(title_label, "FUNCTION TEST");
    lv_obj_set_style_text_color(title_label, UI_MAIN_TEXT, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 10, 0);

    for (item_index = 0; item_index < test_item_count; item_index++) {
        int32_t column = (int32_t)(item_index % 2U);
        int32_t row = (int32_t)(item_index / 2U);
        int32_t x = 8 + column * 156;

        if (item_index == test_item_touch_calibration) {
            x = 86;
        }

        create_test_item_button(screen, (test_item_id_t)item_index,
                                x, 42 + row * 48);
    }

    load_screen(screen, back_animation);
}

void test_menu_ui_create(void)
{
    show_main_menu(false);
}
