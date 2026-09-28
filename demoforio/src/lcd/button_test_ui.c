/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "button_test_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "hpm_gpio_drv.h"

#define BUTTON_UI_BACKGROUND lv_color_hex(0xF3F6FA)
#define BUTTON_UI_SURFACE    lv_color_hex(0xFFFFFF)
#define BUTTON_UI_BORDER     lv_color_hex(0xD6DEE8)
#define BUTTON_UI_TEXT       lv_color_hex(0x172033)
#define BUTTON_UI_MUTED      lv_color_hex(0x64748B)
#define BUTTON_UI_ACTIVE     lv_color_hex(0x22C55E)

#define BUTTON_POLL_PERIOD_MS (10U)
#define BUTTON_DEBOUNCE_COUNT (3U)

typedef enum {
    button_key_1 = 0,
    button_key_2,
    button_key_3,
    button_key_count
} button_key_t;

typedef struct {
    uint32_t pad;
    uint32_t function;
    uint8_t pin;
    const char *name;
    const char *pin_name;
} button_key_config_t;

static const button_key_config_t key_config[button_key_count] = {
    [button_key_1] = {
        .pad = IOC_PAD_PC14,
        .function = IOC_PC14_FUNC_CTL_GPIO_C_14,
        .pin = 14,
        .name = "KEY 1",
        .pin_name = "PC14",
    },
    [button_key_2] = {
        .pad = IOC_PAD_PC15,
        .function = IOC_PC15_FUNC_CTL_GPIO_C_15,
        .pin = 15,
        .name = "KEY 2",
        .pin_name = "PC15",
    },
    [button_key_3] = {
        .pad = IOC_PAD_PC19,
        .function = IOC_PC19_FUNC_CTL_GPIO_C_19,
        .pin = 19,
        .name = "KEY 3",
        .pin_name = "PC19",
    },
};

static lv_timer_t *button_poll_timer;
static lv_obj_t *key_cards[button_key_count];
static lv_obj_t *key_state_labels[button_key_count];
static lv_obj_t *status_box;
static lv_obj_t *status_label;
static bool key_candidate[button_key_count];
static bool key_pressed[button_key_count];
static uint8_t key_stable_count[button_key_count];

static bool read_key_pressed(button_key_t key)
{
    return !gpio_read_pin(HPM_GPIO0, GPIO_DI_GPIOC, key_config[key].pin);
}

static void button_hardware_init(void)
{
    uint32_t index;
    uint32_t pad_ctl = IOC_PAD_PAD_CTL_PE_SET(1) |
                       IOC_PAD_PAD_CTL_PS_SET(1) |
                       IOC_PAD_PAD_CTL_HYS_SET(1);

    for (index = 0; index < button_key_count; index++) {
        HPM_IOC->PAD[key_config[index].pad].FUNC_CTL =
            key_config[index].function;
        HPM_IOC->PAD[key_config[index].pad].PAD_CTL = pad_ctl;
        gpio_set_pin_input(HPM_GPIO0, GPIO_DI_GPIOC,
                           key_config[index].pin);
    }
}

static void update_key_view(button_key_t key)
{
    bool pressed = key_pressed[key];

    if (pressed) {
        lv_obj_add_state(key_cards[key], LV_STATE_CHECKED);
        lv_label_set_text(key_state_labels[key], "PRESSED");
        lv_obj_set_style_text_color(key_state_labels[key],
                                    lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_obj_remove_state(key_cards[key], LV_STATE_CHECKED);
        lv_label_set_text(key_state_labels[key], "RELEASED");
        lv_obj_set_style_text_color(key_state_labels[key], BUTTON_UI_MUTED, 0);
    }
}

static void update_status(button_key_t preferred_key)
{
    uint32_t index;
    int32_t pressed_key = -1;

    if ((preferred_key < button_key_count) && key_pressed[preferred_key]) {
        pressed_key = preferred_key;
    } else {
        for (index = 0; index < button_key_count; index++) {
            if (key_pressed[index]) {
                pressed_key = (int32_t)index;
                break;
            }
        }
    }

    if (pressed_key >= 0) {
        lv_label_set_text_fmt(status_label, "%s PRESSED",
                              key_config[pressed_key].name);
        lv_obj_set_style_border_color(status_box, BUTTON_UI_ACTIVE, 0);
        lv_obj_set_style_text_color(status_label, BUTTON_UI_ACTIVE, 0);
    } else {
        lv_label_set_text(status_label, "PRESS A KEY");
        lv_obj_set_style_border_color(status_box, BUTTON_UI_BORDER, 0);
        lv_obj_set_style_text_color(status_label, BUTTON_UI_MUTED, 0);
    }
}

static void button_poll_timer_cb(lv_timer_t *timer)
{
    uint32_t index;

    LV_UNUSED(timer);
    for (index = 0; index < button_key_count; index++) {
        bool raw_pressed = read_key_pressed((button_key_t)index);

        if (raw_pressed != key_candidate[index]) {
            key_candidate[index] = raw_pressed;
            key_stable_count[index] = 1U;
            continue;
        }
        if (key_stable_count[index] < BUTTON_DEBOUNCE_COUNT) {
            key_stable_count[index]++;
        }
        if ((key_stable_count[index] == BUTTON_DEBOUNCE_COUNT) &&
            (key_pressed[index] != key_candidate[index])) {
            key_pressed[index] = key_candidate[index];
            update_key_view((button_key_t)index);
            update_status((button_key_t)index);
            if (key_pressed[index]) {
                printf("%s (%s) pressed\r\n", key_config[index].name,
                       key_config[index].pin_name);
            }
        }
    }
}

static void create_key_card(lv_obj_t *screen, button_key_t key, int32_t x)
{
    lv_obj_t *card;
    lv_obj_t *label;

    card = lv_button_create(screen);
    lv_obj_set_pos(card, x, 62);
    lv_obj_set_size(card, 96, 106);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_bg_color(card, BUTTON_UI_SURFACE, 0);
    lv_obj_set_style_bg_color(card, BUTTON_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_width(card, 2, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(card, BUTTON_UI_BORDER, 0);
    lv_obj_set_style_border_color(card, BUTTON_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_shadow_offset_y(card, 1, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_text_color(card, BUTTON_UI_TEXT, 0);
    lv_obj_set_style_text_color(card, lv_color_hex(0xFFFFFF),
                                LV_STATE_CHECKED);

    label = lv_label_create(card);
    lv_label_set_text(label, key_config[key].name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 7);

    label = lv_label_create(card);
    lv_label_set_text(label, key_config[key].pin_name);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -1);

    key_state_labels[key] = lv_label_create(card);
    lv_label_set_text(key_state_labels[key], "RELEASED");
    lv_obj_set_style_text_color(key_state_labels[key], BUTTON_UI_MUTED, 0);
    lv_obj_align(key_state_labels[key], LV_ALIGN_BOTTOM_MID, 0, -7);

    key_cards[key] = card;
}

void button_test_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *label;
    uint32_t index;

    button_hardware_init();
    lv_obj_set_style_bg_color(screen, BUTTON_UI_BACKGROUND, 0);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 38);
    lv_obj_set_style_bg_color(header, BUTTON_UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, BUTTON_UI_BORDER, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 5, 4);
    lv_obj_set_size(back_button, 34, 30);
    lv_obj_set_style_radius(back_button, 6, 0);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xE8EDF3), 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(back_button);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, BUTTON_UI_TEXT, 0);
    lv_obj_center(label);

    label = lv_label_create(header);
    lv_label_set_text(label, "BUTTON TEST");
    lv_obj_set_style_text_color(label, BUTTON_UI_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(label, 49, 8);

    create_key_card(screen, button_key_1, 8);
    create_key_card(screen, button_key_2, 112);
    create_key_card(screen, button_key_3, 216);

    status_box = lv_obj_create(screen);
    lv_obj_remove_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(status_box, 8, 184);
    lv_obj_set_size(status_box, 304, 42);
    lv_obj_set_style_radius(status_box, 6, 0);
    lv_obj_set_style_bg_color(status_box, BUTTON_UI_SURFACE, 0);
    lv_obj_set_style_border_width(status_box, 1, 0);
    lv_obj_set_style_border_color(status_box, BUTTON_UI_BORDER, 0);
    lv_obj_set_style_pad_all(status_box, 0, 0);

    status_label = lv_label_create(status_box);
    lv_label_set_text(status_label, "PRESS A KEY");
    lv_obj_set_style_text_color(status_label, BUTTON_UI_MUTED, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_center(status_label);

    for (index = 0; index < button_key_count; index++) {
        key_candidate[index] = read_key_pressed((button_key_t)index);
        key_pressed[index] = key_candidate[index];
        key_stable_count[index] = BUTTON_DEBOUNCE_COUNT;
        update_key_view((button_key_t)index);
    }
    update_status(button_key_count);
    button_poll_timer = lv_timer_create(button_poll_timer_cb,
                                        BUTTON_POLL_PERIOD_MS, NULL);
}

void button_test_ui_stop(void)
{
    uint32_t index;

    if (button_poll_timer != NULL) {
        lv_timer_delete(button_poll_timer);
        button_poll_timer = NULL;
    }
    status_box = NULL;
    status_label = NULL;
    for (index = 0; index < button_key_count; index++) {
        key_cards[index] = NULL;
        key_state_labels[index] = NULL;
    }
}
