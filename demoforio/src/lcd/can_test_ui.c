/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "can_test_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hpm_mcan_drv.h"

#define CAN_UI_BACKGROUND lv_color_hex(0xF3F6FA)
#define CAN_UI_SURFACE    lv_color_hex(0xFFFFFF)
#define CAN_UI_BORDER     lv_color_hex(0xD6DEE8)
#define CAN_UI_TEXT       lv_color_hex(0x172033)
#define CAN_UI_MUTED      lv_color_hex(0x64748B)
#define CAN_UI_ACTIVE     lv_color_hex(0x0D9488)

#define CAN_TEST_BAUDRATE        (500000U)
#define CAN_RX_POLL_PERIOD_MS    (10U)
#define CAN_RX_FRAMES_PER_POLL   (8U)
#define CAN_RX_LINE_COUNT        (4U)
#define CAN_RX_LINE_LENGTH       (48U)
#define CAN_MUTUAL_TIMEOUT_MS    (1000U)
#define CAN4_TO_CAN7_ID          (0x407U)
#define CAN7_TO_CAN4_ID          (0x704U)
#define CAN_MUTUAL_RX_AT_CAN7    (1U << 0)
#define CAN_MUTUAL_RX_AT_CAN4    (1U << 1)
#define CAN_MUTUAL_RX_COMPLETE   (CAN_MUTUAL_RX_AT_CAN7 | CAN_MUTUAL_RX_AT_CAN4)

typedef enum {
    can_port_4 = 0,
    can_port_7,
    can_port_count
} can_port_t;

typedef struct {
    MCAN_Type *can;
    const char *name;
    const char *pins;
    uint8_t test_data[8];
    uint32_t *message_ram;
    uint32_t message_ram_size;
} can_port_config_t;

ATTR_PLACE_AT(".ahb_sram")
static uint32_t can4_message_ram[MCAN_MSG_BUF_SIZE_IN_WORDS];
ATTR_PLACE_AT(".ahb_sram")
static uint32_t can7_message_ram[MCAN_MSG_BUF_SIZE_IN_WORDS];

static const can_port_config_t port_config[can_port_count] = {
    [can_port_4] = {
        .can = HPM_MCAN4,
        .name = "CAN4",
        .pins = "PC16 TX / PC17 RX   500K",
        .test_data = {'C', 'A', 'N', '4', 'T', 'E', 'S', 'T'},
        .message_ram = can4_message_ram,
        .message_ram_size = sizeof(can4_message_ram),
    },
    [can_port_7] = {
        .can = HPM_MCAN7,
        .name = "CAN7",
        .pins = "PC31 TX / PC30 RX   500K",
        .test_data = {'C', 'A', 'N', '7', 'T', 'E', 'S', 'T'},
        .message_ram = can7_message_ram,
        .message_ram_size = sizeof(can7_message_ram),
    },
};

static lv_timer_t *rx_poll_timer;
static lv_obj_t *port_buttons[can_port_count];
static lv_obj_t *pin_label;
static lv_obj_t *frame_label;
static lv_obj_t *activity_label;
static can_port_t active_port;
static bool port_ready[can_port_count];
static bool mutual_test_running;
static uint8_t mutual_rx_mask;
static uint32_t mutual_test_start_tick;
static char frame_lines[CAN_RX_LINE_COUNT][CAN_RX_LINE_LENGTH + 1U];

static const uint8_t can4_to_can7_data[8] =
    {'C', 'A', 'N', '4', '>', '7', '!', '!'};
static const uint8_t can7_to_can4_data[8] =
    {'C', 'A', 'N', '7', '>', '4', '!', '!'};

static void refresh_frame_view(void)
{
    char display[(CAN_RX_LINE_LENGTH + 1U) * CAN_RX_LINE_COUNT];

    snprintf(display, sizeof(display), "%s\n%s\n%s\n%s",
             frame_lines[0], frame_lines[1], frame_lines[2], frame_lines[3]);
    lv_label_set_text(frame_label, display);
}

static void clear_frame_view(void)
{
    memset(frame_lines, 0, sizeof(frame_lines));
    if (frame_label != NULL) {
        refresh_frame_view();
    }
}

static void append_received_frame(can_port_t receiver,
                                  const mcan_rx_message_t *frame)
{
    char *line;
    uint32_t data_length;
    uint32_t index;
    int length;

    memmove(frame_lines[0], frame_lines[1],
            sizeof(frame_lines[0]) * (CAN_RX_LINE_COUNT - 1U));
    line = frame_lines[CAN_RX_LINE_COUNT - 1U];
    memset(line, 0, sizeof(frame_lines[0]));

    if (frame->use_ext_id) {
        length = snprintf(line, CAN_RX_LINE_LENGTH + 1U, "C%u %08lX [%u]",
                          receiver == can_port_4 ? 4U : 7U,
                          (unsigned long)frame->ext_id, frame->dlc);
    } else {
        length = snprintf(line, CAN_RX_LINE_LENGTH + 1U, "C%u %03lX [%u]",
                          receiver == can_port_4 ? 4U : 7U,
                          (unsigned long)frame->std_id, frame->dlc);
    }

    data_length = mcan_get_message_size_from_dlc(frame->dlc);
    if (data_length > 8U) {
        data_length = 8U;
    }
    for (index = 0; (index < data_length) &&
                    (length > 0) && (length < CAN_RX_LINE_LENGTH);
         index++) {
        length += snprintf(&line[length],
                           CAN_RX_LINE_LENGTH + 1U - (uint32_t)length,
                           " %02X", frame->data_8[index]);
    }
}

static bool initialize_port(can_port_t port)
{
    const can_port_config_t *selected = &port_config[port];
    mcan_config_t config;
    mcan_msg_buf_attr_t message_ram = {
        .ram_base = (uint32_t)selected->message_ram,
        .ram_size = selected->message_ram_size,
    };
    uint32_t can_clock;

    board_init_can(selected->can);
    can_clock = board_init_can_clock(selected->can);
    if ((can_clock == 0U) ||
        (mcan_set_msg_buf_attr(selected->can, &message_ram) !=
         status_success)) {
        return false;
    }

    mcan_get_default_config(selected->can, &config);
    config.baudrate = CAN_TEST_BAUDRATE;
    config.mode = mcan_mode_normal;
    config.enable_canfd = false;
    return mcan_init(selected->can, &config, can_clock) == status_success;
}

static void set_activity(const char *text, bool success)
{
    lv_label_set_text(activity_label, text);
    lv_obj_set_style_text_color(activity_label,
                                success ? CAN_UI_ACTIVE
                                        : lv_color_hex(0xDC2626),
                                0);
}

static void select_port(can_port_t port)
{
    uint32_t index;

    active_port = port;
    for (index = 0; index < can_port_count; index++) {
        if (index == (uint32_t)port) {
            lv_obj_add_state(port_buttons[index], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(port_buttons[index], LV_STATE_CHECKED);
        }
    }

    lv_label_set_text(pin_label, port_config[port].pins);
    set_activity(port_ready[port] ? "READY" : "ERROR", port_ready[port]);
}

static void port_button_event_cb(lv_event_t *event)
{
    can_port_t port;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    port = (can_port_t)(uintptr_t)lv_event_get_user_data(event);
    select_port(port);
}

static void send_button_event_cb(lv_event_t *event)
{
    mcan_tx_frame_t frame = {0};

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (!port_ready[active_port]) {
        set_activity("ERROR", false);
        return;
    }

    frame.std_id = 0x123U;
    frame.dlc = 8U;
    memcpy(frame.data_8, port_config[active_port].test_data,
           sizeof(port_config[active_port].test_data));
    if (mcan_transmit_via_txfifo_nonblocking(port_config[active_port].can,
                                              &frame, NULL) ==
        status_success) {
        set_activity("TX OK", true);
    } else {
        set_activity("TX ERR", false);
    }
}

static hpm_stat_t transmit_mutual_frame(can_port_t sender, uint32_t id,
                                        const uint8_t data[8])
{
    mcan_tx_frame_t frame = {0};

    frame.std_id = id;
    frame.dlc = 8U;
    memcpy(frame.data_8, data, sizeof(frame.data_8[0]) * 8U);
    return mcan_transmit_via_txfifo_nonblocking(port_config[sender].can,
                                                &frame, NULL);
}

static void mutual_button_event_cb(lv_event_t *event)
{
    hpm_stat_t can4_status;
    hpm_stat_t can7_status;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (!port_ready[can_port_4] || !port_ready[can_port_7]) {
        set_activity("ERROR", false);
        return;
    }

    mutual_test_running = false;
    mutual_rx_mask = 0U;
    can4_status = transmit_mutual_frame(can_port_4, CAN4_TO_CAN7_ID,
                                        can4_to_can7_data);
    can7_status = transmit_mutual_frame(can_port_7, CAN7_TO_CAN4_ID,
                                        can7_to_can4_data);
    if ((can4_status == status_success) && (can7_status == status_success)) {
        mutual_test_start_tick = lv_tick_get();
        mutual_test_running = true;
        set_activity("WAIT", true);
    } else {
        set_activity("FAIL", false);
    }
}

static void clear_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        clear_frame_view();
        mutual_test_running = false;
        mutual_rx_mask = 0U;
        set_activity(port_ready[active_port] ? "READY" : "ERROR",
                     port_ready[active_port]);
    }
}

static void check_mutual_frame(can_port_t receiver,
                               const mcan_rx_message_t *frame)
{
    if (!mutual_test_running || frame->use_ext_id || (frame->dlc != 8U)) {
        return;
    }

    if ((receiver == can_port_7) && (frame->std_id == CAN4_TO_CAN7_ID) &&
        (memcmp(frame->data_8, can4_to_can7_data,
                sizeof(can4_to_can7_data)) == 0)) {
        mutual_rx_mask |= CAN_MUTUAL_RX_AT_CAN7;
    }
    if ((receiver == can_port_4) && (frame->std_id == CAN7_TO_CAN4_ID) &&
        (memcmp(frame->data_8, can7_to_can4_data,
                sizeof(can7_to_can4_data)) == 0)) {
        mutual_rx_mask |= CAN_MUTUAL_RX_AT_CAN4;
    }

    if (mutual_rx_mask == CAN_MUTUAL_RX_COMPLETE) {
        mutual_test_running = false;
        set_activity("PASS", true);
    }
}

static void rx_poll_timer_cb(lv_timer_t *timer)
{
    mcan_rx_message_t frame;
    uint32_t received = 0U;
    uint32_t port_index;
    uint32_t fifo_index;

    LV_UNUSED(timer);

    for (port_index = 0U;
         (port_index < can_port_count) &&
         (received < CAN_RX_FRAMES_PER_POLL);
         port_index++) {
        if (!port_ready[port_index]) {
            continue;
        }
        for (fifo_index = 0U;
             (fifo_index < 2U) && (received < CAN_RX_FRAMES_PER_POLL);
             fifo_index++) {
            while ((received < CAN_RX_FRAMES_PER_POLL) &&
                   (mcan_read_rxfifo(port_config[port_index].can, fifo_index,
                                     &frame) == status_success)) {
                append_received_frame((can_port_t)port_index, &frame);
                check_mutual_frame((can_port_t)port_index, &frame);
                received++;
            }
        }
    }
    if (received > 0U) {
        refresh_frame_view();
        if (!mutual_test_running &&
            (mutual_rx_mask != CAN_MUTUAL_RX_COMPLETE)) {
            set_activity("RX", true);
        }
    }
    if (mutual_test_running &&
        (lv_tick_elaps(mutual_test_start_tick) > CAN_MUTUAL_TIMEOUT_MS)) {
        mutual_test_running = false;
        set_activity("FAIL", false);
    }
}

static void create_port_button(lv_obj_t *screen, can_port_t port, int32_t x)
{
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, 42);
    lv_obj_set_size(button, 148, 34);
    lv_obj_set_style_radius(button, 5, 0);
    lv_obj_set_style_bg_color(button, CAN_UI_SURFACE, 0);
    lv_obj_set_style_bg_color(button, CAN_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, CAN_UI_BORDER, 0);
    lv_obj_set_style_border_color(button, CAN_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, CAN_UI_TEXT, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF),
                                LV_STATE_CHECKED);
    lv_obj_add_event_cb(button, port_button_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)port);

    label = lv_label_create(button);
    lv_label_set_text(label, port_config[port].name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);

    port_buttons[port] = button;
}

void can_test_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *label;
    lv_obj_t *frame_view;
    lv_obj_t *send_button;
    lv_obj_t *mutual_button;
    lv_obj_t *clear_button;
    uint32_t index;

    lv_obj_set_style_bg_color(screen, CAN_UI_BACKGROUND, 0);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 38);
    lv_obj_set_style_bg_color(header, CAN_UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, CAN_UI_BORDER, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 5, 4);
    lv_obj_set_size(back_button, 34, 30);
    lv_obj_set_style_radius(back_button, 6, 0);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xE8EDF3), 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(back_button);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, CAN_UI_TEXT, 0);
    lv_obj_center(label);

    label = lv_label_create(header);
    lv_label_set_text(label, "CAN TEST");
    lv_obj_set_style_text_color(label, CAN_UI_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(label, 49, 8);

    create_port_button(screen, can_port_4, 8);
    create_port_button(screen, can_port_7, 164);

    pin_label = lv_label_create(screen);
    lv_obj_set_width(pin_label, 304);
    lv_obj_set_style_text_color(pin_label, CAN_UI_MUTED, 0);
    lv_obj_set_style_text_font(pin_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pin_label, 8, 80);

    frame_view = lv_obj_create(screen);
    lv_obj_remove_flag(frame_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(frame_view, 8, 99);
    lv_obj_set_size(frame_view, 304, 87);
    lv_obj_set_style_radius(frame_view, 5, 0);
    lv_obj_set_style_bg_color(frame_view, CAN_UI_SURFACE, 0);
    lv_obj_set_style_border_width(frame_view, 1, 0);
    lv_obj_set_style_border_color(frame_view, CAN_UI_BORDER, 0);
    lv_obj_set_style_pad_all(frame_view, 6, 0);

    frame_label = lv_label_create(frame_view);
    lv_obj_set_width(frame_label, 290);
    lv_obj_set_style_text_color(frame_label, CAN_UI_TEXT, 0);
    lv_obj_set_style_text_font(frame_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(frame_label, 2, 0);
    lv_obj_set_pos(frame_label, 0, 0);

    send_button = lv_button_create(screen);
    lv_obj_set_pos(send_button, 8, 194);
    lv_obj_set_size(send_button, 84, 38);
    lv_obj_set_style_radius(send_button, 6, 0);
    lv_obj_set_style_bg_color(send_button, CAN_UI_ACTIVE, 0);
    lv_obj_set_style_shadow_width(send_button, 0, 0);
    lv_obj_add_event_cb(send_button, send_button_event_cb, LV_EVENT_CLICKED,
                        NULL);

    label = lv_label_create(send_button);
    lv_label_set_text(label, "SEND");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    mutual_button = lv_button_create(screen);
    lv_obj_set_pos(mutual_button, 100, 194);
    lv_obj_set_size(mutual_button, 96, 38);
    lv_obj_set_style_radius(mutual_button, 6, 0);
    lv_obj_set_style_bg_color(mutual_button, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_shadow_width(mutual_button, 0, 0);
    lv_obj_add_event_cb(mutual_button, mutual_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    label = lv_label_create(mutual_button);
    lv_label_set_text(label, "MUTUAL");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    clear_button = lv_button_create(screen);
    lv_obj_set_pos(clear_button, 204, 194);
    lv_obj_set_size(clear_button, 60, 38);
    lv_obj_set_style_radius(clear_button, 6, 0);
    lv_obj_set_style_bg_color(clear_button, CAN_UI_SURFACE, 0);
    lv_obj_set_style_border_width(clear_button, 1, 0);
    lv_obj_set_style_border_color(clear_button, CAN_UI_BORDER, 0);
    lv_obj_set_style_shadow_width(clear_button, 0, 0);
    lv_obj_add_event_cb(clear_button, clear_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    label = lv_label_create(clear_button);
    lv_label_set_text(label, "CLEAR");
    lv_obj_set_style_text_color(label, CAN_UI_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    activity_label = lv_label_create(screen);
    lv_obj_set_width(activity_label, 44);
    lv_obj_set_style_text_align(activity_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(activity_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(activity_label, 268, 205);

    clear_frame_view();
    memset(port_ready, 0, sizeof(port_ready));
    mutual_test_running = false;
    mutual_rx_mask = 0U;
    for (index = 0; index < can_port_count; index++) {
        port_ready[index] = initialize_port((can_port_t)index);
    }
    select_port(can_port_4);
    rx_poll_timer = lv_timer_create(rx_poll_timer_cb,
                                    CAN_RX_POLL_PERIOD_MS, NULL);
}

void can_test_ui_stop(void)
{
    uint32_t index;

    if (rx_poll_timer != NULL) {
        lv_timer_delete(rx_poll_timer);
        rx_poll_timer = NULL;
    }
    for (index = 0; index < can_port_count; index++) {
        if (port_ready[index]) {
            mcan_deinit(port_config[index].can);
            port_ready[index] = false;
        }
    }
    pin_label = NULL;
    frame_label = NULL;
    activity_label = NULL;
    for (index = 0; index < can_port_count; index++) {
        port_buttons[index] = NULL;
    }
}
