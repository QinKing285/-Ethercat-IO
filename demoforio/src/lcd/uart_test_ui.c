/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "uart_test_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_uart_drv.h"

#define UART_UI_BACKGROUND lv_color_hex(0xF3F6FA)
#define UART_UI_SURFACE    lv_color_hex(0xFFFFFF)
#define UART_UI_BORDER     lv_color_hex(0xD6DEE8)
#define UART_UI_TEXT       lv_color_hex(0x172033)
#define UART_UI_MUTED      lv_color_hex(0x64748B)
#define UART_UI_ACTIVE     lv_color_hex(0x0284C7)

#define UART_TEST_BAUDRATE       (115200U)
#define UART_RX_POLL_PERIOD_MS   (10U)
#define UART_RX_BYTES_PER_POLL   (128U)
#define UART_RX_LINE_COUNT       (4U)
#define UART_RX_LINE_LENGTH      (34U)

typedef enum {
    uart_port_0 = 0,
    uart_port_12,
    uart_port_13,
    uart_port_14,
    uart_port_count
} uart_port_t;

typedef struct {
    UART_Type *uart;
    clock_name_t clock_name;
    uint32_t tx_pad;
    uint32_t rx_pad;
    uint32_t tx_function;
    uint32_t rx_function;
    const char *name;
    const char *pins;
} uart_port_config_t;

static const uart_port_config_t port_config[uart_port_count] = {
    [uart_port_0] = {
        .uart = HPM_UART0,
        .clock_name = clock_uart0,
        .tx_pad = IOC_PAD_PA00,
        .rx_pad = IOC_PAD_PA01,
        .tx_function = IOC_PA00_FUNC_CTL_UART0_TXD,
        .rx_function = IOC_PA01_FUNC_CTL_UART0_RXD,
        .name = "UART0",
        .pins = "PA0 TX / PA1 RX   115200 8N1",
    },
    [uart_port_12] = {
        .uart = HPM_UART12,
        .clock_name = clock_uart12,
        .tx_pad = IOC_PAD_PD16,
        .rx_pad = IOC_PAD_PD17,
        .tx_function = IOC_PD16_FUNC_CTL_UART12_TXD,
        .rx_function = IOC_PD17_FUNC_CTL_UART12_RXD,
        .name = "UART12",
        .pins = "PD16 TX / PD17 RX   115200 8N1",
    },
    [uart_port_13] = {
        .uart = HPM_UART13,
        .clock_name = clock_uart13,
        .tx_pad = IOC_PAD_PD23,
        .rx_pad = IOC_PAD_PD22,
        .tx_function = IOC_PD23_FUNC_CTL_UART13_TXD,
        .rx_function = IOC_PD22_FUNC_CTL_UART13_RXD,
        .name = "UART13",
        .pins = "PD23 TX / PD22 RX   115200 8N1",
    },
    [uart_port_14] = {
        .uart = HPM_UART14,
        .clock_name = clock_uart14,
        .tx_pad = IOC_PAD_PD24,
        .rx_pad = IOC_PAD_PD25,
        .tx_function = IOC_PD24_FUNC_CTL_UART14_TXD,
        .rx_function = IOC_PD25_FUNC_CTL_UART14_RXD,
        .name = "UART14",
        .pins = "PD24 TX / PD25 RX   115200 8N1",
    },
};

static lv_timer_t *rx_poll_timer;
static lv_obj_t *port_buttons[uart_port_count];
static lv_obj_t *pin_label;
static lv_obj_t *terminal_label;
static lv_obj_t *activity_label;
static uart_port_t active_port;
static bool active_port_ready;
static char rx_lines[UART_RX_LINE_COUNT][UART_RX_LINE_LENGTH + 1U];
static uint8_t rx_column;

static void refresh_terminal(void)
{
    char display[(UART_RX_LINE_LENGTH + 1U) * UART_RX_LINE_COUNT];

    snprintf(display, sizeof(display), "%s\n%s\n%s\n%s",
             rx_lines[0], rx_lines[1], rx_lines[2], rx_lines[3]);
    lv_label_set_text(terminal_label, display);
}

static void clear_terminal(void)
{
    memset(rx_lines, 0, sizeof(rx_lines));
    rx_column = 0U;
    if (terminal_label != NULL) {
        refresh_terminal();
    }
}

static void advance_rx_line(void)
{
    memmove(rx_lines[0], rx_lines[1],
            sizeof(rx_lines[0]) * (UART_RX_LINE_COUNT - 1U));
    memset(rx_lines[UART_RX_LINE_COUNT - 1U], 0,
           sizeof(rx_lines[UART_RX_LINE_COUNT - 1U]));
    rx_column = 0U;
}

static void append_rx_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        advance_rx_line();
        return;
    }
    if (rx_column >= UART_RX_LINE_LENGTH) {
        advance_rx_line();
    }
    if ((byte < 0x20U) || (byte > 0x7EU)) {
        byte = '.';
    }
    rx_lines[UART_RX_LINE_COUNT - 1U][rx_column++] = (char)byte;
    rx_lines[UART_RX_LINE_COUNT - 1U][rx_column] = '\0';
}

static bool initialize_port(uart_port_t port)
{
    const uart_port_config_t *selected = &port_config[port];
    uart_config_t config = {0};
    uint32_t rx_pad_ctl = IOC_PAD_PAD_CTL_PE_SET(1) |
                          IOC_PAD_PAD_CTL_PS_SET(1) |
                          IOC_PAD_PAD_CTL_HYS_SET(1);

    HPM_IOC->PAD[selected->tx_pad].FUNC_CTL = selected->tx_function;
    HPM_IOC->PAD[selected->rx_pad].FUNC_CTL = selected->rx_function;
    HPM_IOC->PAD[selected->rx_pad].PAD_CTL = rx_pad_ctl;

    clock_add_to_group(selected->clock_name, 0);
    uart_default_config(selected->uart, &config);
    config.src_freq_in_hz = clock_get_frequency(selected->clock_name);
    config.baudrate = UART_TEST_BAUDRATE;
    if (uart_init(selected->uart, &config) != status_success) {
        return false;
    }
    uart_clear_rx_fifo(selected->uart);
    return true;
}

static void set_activity(const char *text, bool success)
{
    lv_label_set_text(activity_label, text);
    lv_obj_set_style_text_color(activity_label,
                                success ? UART_UI_ACTIVE
                                        : lv_color_hex(0xDC2626),
                                0);
}

static void select_port(uart_port_t port)
{
    uint32_t index;

    active_port = port;
    for (index = 0; index < uart_port_count; index++) {
        if (index == (uint32_t)port) {
            lv_obj_add_state(port_buttons[index], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(port_buttons[index], LV_STATE_CHECKED);
        }
    }

    lv_label_set_text(pin_label, port_config[port].pins);
    clear_terminal();
    active_port_ready = initialize_port(port);
    set_activity(active_port_ready ? "READY" : "ERROR", active_port_ready);
}

static void port_button_event_cb(lv_event_t *event)
{
    uart_port_t port;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    port = (uart_port_t)(uintptr_t)lv_event_get_user_data(event);
    select_port(port);
}

static void send_button_event_cb(lv_event_t *event)
{
    char message[32];
    int length;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (!active_port_ready) {
        set_activity("ERROR", false);
        return;
    }

    length = snprintf(message, sizeof(message), "%s TX TEST\r\n",
                      port_config[active_port].name);
    if ((length > 0) &&
        (uart_send_data(port_config[active_port].uart, (uint8_t *)message,
                        (uint32_t)length) == status_success)) {
        set_activity("TX OK", true);
    } else {
        set_activity("TX ERR", false);
    }
}

static void clear_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        clear_terminal();
        if (active_port_ready) {
            uart_clear_rx_fifo(port_config[active_port].uart);
        }
        set_activity(active_port_ready ? "READY" : "ERROR",
                     active_port_ready);
    }
}

static void rx_poll_timer_cb(lv_timer_t *timer)
{
    uint32_t received = 0U;
    uint8_t byte;

    LV_UNUSED(timer);
    if (!active_port_ready) {
        return;
    }

    while ((received < UART_RX_BYTES_PER_POLL) &&
           (uart_try_receive_byte(port_config[active_port].uart, &byte) ==
            status_success)) {
        append_rx_byte(byte);
        received++;
    }
    if (received > 0U) {
        refresh_terminal();
        set_activity("RX DATA", true);
    }
}

static void create_port_button(lv_obj_t *screen, uart_port_t port, int32_t x)
{
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, 42);
    lv_obj_set_size(button, 75, 32);
    lv_obj_set_style_radius(button, 5, 0);
    lv_obj_set_style_bg_color(button, UART_UI_SURFACE, 0);
    lv_obj_set_style_bg_color(button, UART_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, UART_UI_BORDER, 0);
    lv_obj_set_style_border_color(button, UART_UI_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, UART_UI_TEXT, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF),
                                LV_STATE_CHECKED);
    lv_obj_add_event_cb(button, port_button_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)port);

    label = lv_label_create(button);
    lv_label_set_text(label, port_config[port].name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    port_buttons[port] = button;
}

void uart_test_ui_create(lv_obj_t *screen, lv_event_cb_t back_event_cb)
{
    lv_obj_t *header;
    lv_obj_t *back_button;
    lv_obj_t *label;
    lv_obj_t *terminal;
    lv_obj_t *send_button;
    lv_obj_t *clear_button;

    lv_obj_set_style_bg_color(screen, UART_UI_BACKGROUND, 0);

    header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 320, 38);
    lv_obj_set_style_bg_color(header, UART_UI_SURFACE, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, UART_UI_BORDER, 0);

    back_button = lv_button_create(header);
    lv_obj_set_pos(back_button, 5, 4);
    lv_obj_set_size(back_button, 34, 30);
    lv_obj_set_style_radius(back_button, 6, 0);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xE8EDF3), 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(back_button);
    lv_label_set_text(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, UART_UI_TEXT, 0);
    lv_obj_center(label);

    label = lv_label_create(header);
    lv_label_set_text(label, "UART TEST");
    lv_obj_set_style_text_color(label, UART_UI_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(label, 49, 8);

    create_port_button(screen, uart_port_0, 5);
    create_port_button(screen, uart_port_12, 83);
    create_port_button(screen, uart_port_13, 161);
    create_port_button(screen, uart_port_14, 239);

    pin_label = lv_label_create(screen);
    lv_obj_set_width(pin_label, 304);
    lv_obj_set_style_text_color(pin_label, UART_UI_MUTED, 0);
    lv_obj_set_style_text_font(pin_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pin_label, 8, 78);

    terminal = lv_obj_create(screen);
    lv_obj_remove_flag(terminal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(terminal, 8, 97);
    lv_obj_set_size(terminal, 304, 89);
    lv_obj_set_style_radius(terminal, 5, 0);
    lv_obj_set_style_bg_color(terminal, UART_UI_SURFACE, 0);
    lv_obj_set_style_border_width(terminal, 1, 0);
    lv_obj_set_style_border_color(terminal, UART_UI_BORDER, 0);
    lv_obj_set_style_pad_all(terminal, 6, 0);

    terminal_label = lv_label_create(terminal);
    lv_obj_set_width(terminal_label, 290);
    lv_obj_set_style_text_color(terminal_label, UART_UI_TEXT, 0);
    lv_obj_set_style_text_font(terminal_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(terminal_label, 2, 0);
    lv_obj_set_pos(terminal_label, 0, 0);

    send_button = lv_button_create(screen);
    lv_obj_set_pos(send_button, 8, 194);
    lv_obj_set_size(send_button, 148, 38);
    lv_obj_set_style_radius(send_button, 6, 0);
    lv_obj_set_style_bg_color(send_button, UART_UI_ACTIVE, 0);
    lv_obj_set_style_shadow_width(send_button, 0, 0);
    lv_obj_add_event_cb(send_button, send_button_event_cb, LV_EVENT_CLICKED,
                        NULL);

    label = lv_label_create(send_button);
    lv_label_set_text(label, "SEND TEST");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);

    clear_button = lv_button_create(screen);
    lv_obj_set_pos(clear_button, 164, 194);
    lv_obj_set_size(clear_button, 72, 38);
    lv_obj_set_style_radius(clear_button, 6, 0);
    lv_obj_set_style_bg_color(clear_button, UART_UI_SURFACE, 0);
    lv_obj_set_style_border_width(clear_button, 1, 0);
    lv_obj_set_style_border_color(clear_button, UART_UI_BORDER, 0);
    lv_obj_set_style_shadow_width(clear_button, 0, 0);
    lv_obj_add_event_cb(clear_button, clear_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    label = lv_label_create(clear_button);
    lv_label_set_text(label, "CLEAR");
    lv_obj_set_style_text_color(label, UART_UI_TEXT, 0);
    lv_obj_center(label);

    activity_label = lv_label_create(screen);
    lv_obj_set_width(activity_label, 68);
    lv_obj_set_style_text_align(activity_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(activity_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(activity_label, 244, 205);

    clear_terminal();
    select_port(uart_port_0);
    rx_poll_timer = lv_timer_create(rx_poll_timer_cb,
                                    UART_RX_POLL_PERIOD_MS, NULL);
}

void uart_test_ui_stop(void)
{
    uint32_t index;

    if (rx_poll_timer != NULL) {
        lv_timer_delete(rx_poll_timer);
        rx_poll_timer = NULL;
    }
    pin_label = NULL;
    terminal_label = NULL;
    activity_label = NULL;
    for (index = 0; index < uart_port_count; index++) {
        port_buttons[index] = NULL;
    }
}
