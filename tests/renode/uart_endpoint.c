// SPDX-License-Identifier: MIT
/* The emulated UART provides Studio frames; USB and BLE are disabled. */
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/studio/rpc.h>
#include <zmk/usb.h>

ZMK_EVENT_IMPL(zmk_usb_conn_state_changed);
enum zmk_usb_conn_state zmk_usb_get_conn_state(void) {
    /* Wired split polling starts when the central has host power. */
    return ZMK_USB_CONN_POWERED;
}

static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

static void receive(void *a, void *b, void *c) {
    k_sleep(K_MSEC(50));
    raise_zmk_usb_conn_state_changed((struct zmk_usb_conn_state_changed){.conn_state = ZMK_USB_CONN_POWERED});
    struct ring_buf *rx = zmk_rpc_get_rx_buf();
    for (;;) {
        uint8_t byte;
        if (ring_buf_space_get(rx) && uart_poll_in(uart, &byte) == 0) {
            ring_buf_put(rx, &byte, 1);
            zmk_rpc_rx_notify();
        } else {
            k_sleep(K_MSEC(1));
        }
    }
}
K_THREAD_DEFINE(mkb_sim_rx, 1024, receive, NULL, NULL, NULL, 8, 0, 0);

static void transmit(struct ring_buf *tx, size_t written, bool done, void *data) {
    uint8_t byte;
    while (ring_buf_get(tx, &byte, 1)) {
        uart_poll_out(uart, byte);
    }
}
ZMK_RPC_TRANSPORT(mkb_sim, ZMK_TRANSPORT_NONE, NULL, NULL, NULL, transmit);
