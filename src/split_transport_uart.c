#include "split_transport.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(split_uart);

#define SPLIT_MAGIC 0xa5
#define SPLIT_VERSION 2
#define SPLIT_EVENT_SIZE 3
#define SPLIT_PAYLOAD_SIZE (SPLIT_TRANSPORT_MAX_EVENTS * SPLIT_EVENT_SIZE)
#define SPLIT_PACKET_SIZE (3 + SPLIT_PAYLOAD_SIZE + 1)

#if DT_HAS_CHOSEN(ergodox_split_uart)
#define SPLIT_UART_NODE DT_CHOSEN(ergodox_split_uart)
#else
#define SPLIT_UART_NODE DT_NODELABEL(uart0)
#endif

static const struct device *const split_uart = DEVICE_DT_GET(SPLIT_UART_NODE);
static split_events_cb_t events_cb;
static void *events_user_data;
static uint8_t rx_buf[SPLIT_PACKET_SIZE];
static uint8_t rx_pos;

static uint8_t tx_buf[SPLIT_PACKET_SIZE];
static uint8_t tx_pending_buf[SPLIT_PACKET_SIZE];
static uint8_t tx_pos;
static uint8_t tx_len;
static bool tx_busy;
static bool tx_pending;
static struct k_spinlock tx_lock;

static uint8_t packet_checksum(const uint8_t *packet)
{
    uint8_t checksum = 0;

    for (int i = 0; i < SPLIT_PACKET_SIZE - 1; i++) {
        checksum ^= packet[i];
    }

    return checksum;
}

static void encode_packet(uint8_t packet[SPLIT_PACKET_SIZE],
    const struct split_key_event *events, uint8_t count)
{
    memset(packet, 0, SPLIT_PACKET_SIZE);
    packet[0] = SPLIT_MAGIC;
    packet[1] = SPLIT_VERSION;
    packet[2] = count;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t *event = &packet[3 + i * SPLIT_EVENT_SIZE];

        event[0] = events[i].hand;
        event[1] = events[i].key_index;
        event[2] = events[i].type;
    }

    packet[SPLIT_PACKET_SIZE - 1] = packet_checksum(packet);
}

static int append_events_to_packet(uint8_t packet[SPLIT_PACKET_SIZE],
    const struct split_key_event *events, uint8_t count)
{
    uint8_t old_count = packet[2];

    if (old_count + count > SPLIT_TRANSPORT_MAX_EVENTS) {
        return -ENOMEM;
    }

    for (uint8_t i = 0; i < count; i++) {
        uint8_t *event = &packet[3 + (old_count + i) * SPLIT_EVENT_SIZE];

        event[0] = events[i].hand;
        event[1] = events[i].key_index;
        event[2] = events[i].type;
    }

    packet[2] = old_count + count;
    packet[SPLIT_PACKET_SIZE - 1] = packet_checksum(packet);

    return 0;
}

static bool decode_packet(const uint8_t packet[SPLIT_PACKET_SIZE],
    struct split_key_event events[SPLIT_TRANSPORT_MAX_EVENTS], uint8_t *count)
{
    if (packet[0] != SPLIT_MAGIC || packet[1] != SPLIT_VERSION ||
        packet[SPLIT_PACKET_SIZE - 1] != packet_checksum(packet)) {
        return false;
    }

    if (packet[2] > SPLIT_TRANSPORT_MAX_EVENTS) {
        return false;
    }

    *count = packet[2];

    for (uint8_t i = 0; i < *count; i++) {
        const uint8_t *event = &packet[3 + i * SPLIT_EVENT_SIZE];

        events[i].hand = event[0];
        events[i].key_index = event[1];
        events[i].type = event[2];
    }

    return true;
}

static void process_rx_byte(uint8_t byte)
{
    struct split_key_event events[SPLIT_TRANSPORT_MAX_EVENTS];
    uint8_t count;

    if (rx_pos == 0 && byte != SPLIT_MAGIC) {
        return;
    }

    rx_buf[rx_pos++] = byte;
    if (rx_pos < SPLIT_PACKET_SIZE) {
        return;
    }

    rx_pos = 0;

    if (!decode_packet(rx_buf, events, &count)) {
        LOG_WRN("Dropped invalid split event packet");
        return;
    }

    if (events_cb != NULL) {
        events_cb(events, count, events_user_data);
    }
}

static void handle_tx_ready(const struct device *dev)
{
    while (uart_irq_tx_ready(dev)) {
        k_spinlock_key_t key = k_spin_lock(&tx_lock);
        uint8_t remaining = tx_len - tx_pos;

        if (remaining == 0) {
            if (tx_pending) {
                memcpy(tx_buf, tx_pending_buf, sizeof(tx_buf));
                tx_pending = false;
                tx_pos = 0;
                tx_len = SPLIT_PACKET_SIZE;
            } else {
                tx_busy = false;
                tx_len = 0;
                tx_pos = 0;
                uart_irq_tx_disable(dev);
            }
            k_spin_unlock(&tx_lock, key);
            return;
        }

        uint8_t written = uart_fifo_fill(dev, &tx_buf[tx_pos], remaining);
        tx_pos += written;
        k_spin_unlock(&tx_lock, key);

        if (written == 0) {
            return;
        }
    }
}

static void split_uart_irq_callback(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    for (;;) {
        uart_irq_update(dev);
        if (!uart_irq_is_pending(dev)) {
            break;
        }

        if (uart_irq_rx_ready(dev)) {
            uint8_t byte;

            while (uart_fifo_read(dev, &byte, 1) == 1) {
                process_rx_byte(byte);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            handle_tx_ready(dev);
        }
    }
}

int split_transport_init(split_events_cb_t cb, void *user_data)
{
    int ret;
    const struct uart_config cfg = {
        .baudrate = CONFIG_ERGODOX_SPLIT_UART_BAUDRATE,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };

    if (!device_is_ready(split_uart)) {
        LOG_ERR("Split UART is not ready");
        return -ENODEV;
    }

    ret = uart_configure(split_uart, &cfg);
    if (ret != 0) {
        LOG_ERR("Failed to configure split UART: %d", ret);
        return ret;
    }

    events_cb = cb;
    events_user_data = user_data;

    ret = uart_irq_callback_user_data_set(split_uart, split_uart_irq_callback,
        NULL);
    if (ret != 0) {
        LOG_ERR("Failed to set split UART callback: %d", ret);
        return ret;
    }

    uart_irq_rx_enable(split_uart);

    return 0;
}

int split_transport_send_events(const struct split_key_event *events,
    uint8_t count)
{
    if ((events == NULL && count != 0) ||
        count > SPLIT_TRANSPORT_MAX_EVENTS) {
        return -EINVAL;
    }

    if (!device_is_ready(split_uart)) {
        return -ENODEV;
    }

    k_spinlock_key_t key = k_spin_lock(&tx_lock);

    if (tx_busy) {
        if (tx_pending) {
            int ret = append_events_to_packet(tx_pending_buf, events, count);

            k_spin_unlock(&tx_lock, key);
            return ret;
        }

        encode_packet(tx_pending_buf, events, count);
        tx_pending = true;
        k_spin_unlock(&tx_lock, key);
        return 0;
    }

    encode_packet(tx_buf, events, count);
    tx_pos = 0;
    tx_len = SPLIT_PACKET_SIZE;
    tx_busy = true;

    k_spin_unlock(&tx_lock, key);
    uart_irq_tx_enable(split_uart);

    return 0;
}

int split_transport_send_event(const struct split_key_event *event)
{
    return split_transport_send_events(event, 1);
}
