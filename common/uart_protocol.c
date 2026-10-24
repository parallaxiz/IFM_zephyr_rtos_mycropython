/**
 * @file uart_protocol.c
 * @brief Framed UART protocol implementation with CRC-16/CCITT.
 */

#include "uart_protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* ── CRC-16/CCITT (poly=0x1021, init=0xFFFF) ────────────────────────────── */
/* ── CRC-16/CCITT (poly=0x1021) ─────────────────────────────────────────── */
uint16_t uart_proto_crc16_update(uint16_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (crc << 1) ^ 0x1021u;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t uart_proto_crc16(const uint8_t *data, size_t length)
{
    return uart_proto_crc16_update(0xFFFFu, data, length);
}

/* ── Internal: read one byte with timeout ───────────────────────────────── */
static int read_byte_timeout(const struct device *uart_dev,
                             uint8_t *byte, uint32_t timeout_ms)
{
    int64_t deadline = k_uptime_get() + (int64_t)timeout_ms;
    while (k_uptime_get() < deadline) {
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            *byte = (uint8_t)c;
            return 0;
        }
        k_sleep(K_MSEC(1));
    }
    return UART_PROTO_ERR_TIMEOUT;
}

/* ── Receive a complete framed script ───────────────────────────────────── */
int uart_proto_receive_frame(const struct device *uart_dev,
                             char *out_buf, size_t buf_size,
                             size_t *out_len)
{
    if (uart_dev == NULL || out_buf == NULL || out_len == NULL) {
        return UART_PROTO_ERR_DEVICE;
    }
    if (!device_is_ready(uart_dev)) {
        printk("[PROTO] Error: UART device not ready\n");
        return UART_PROTO_ERR_DEVICE;
    }

    *out_len = 0;
    uint8_t byte;

    /* ── Step 1: Wait for START byte (0xAA) ─────────────────────────────── */
    printk("[PROTO] Waiting for frame start (0xAA)...\n");
    while (1) {
        int ret = read_byte_timeout(uart_dev, &byte, UART_PROTO_RX_TIMEOUT_MS);
        if (ret != 0) {
            printk("[PROTO] Timeout waiting for start byte\n");
            return UART_PROTO_ERR_TIMEOUT;
        }
        if (byte == UART_PROTO_START_BYTE) {
            break;
        }
        /* Ignore non-start bytes (sync noise) */
    }

    /* ── Step 2: Read 2-byte big-endian payload length ───────────────────── */
    uint8_t len_h, len_l;
    if (read_byte_timeout(uart_dev, &len_h, UART_PROTO_RX_TIMEOUT_MS) != 0 ||
        read_byte_timeout(uart_dev, &len_l, UART_PROTO_RX_TIMEOUT_MS) != 0) {
        printk("[PROTO] Timeout reading length bytes\n");
        return UART_PROTO_ERR_TIMEOUT;
    }
    uint16_t payload_len = ((uint16_t)len_h << 8) | (uint16_t)len_l;

    if (payload_len > UART_PROTO_MAX_PAYLOAD) {
        printk("[PROTO] Error: payload length %u out of range (max %u)\n",
               (unsigned)payload_len, UART_PROTO_MAX_PAYLOAD);
        return UART_PROTO_ERR_TOO_LARGE;
    }
    if (payload_len + 1 > buf_size) {
        printk("[PROTO] Error: output buffer too small (%u needed, %u available)\n",
               (unsigned)(payload_len + 1), (unsigned)buf_size);
        return UART_PROTO_ERR_TOO_LARGE;
    }

    /* ── Step 3: Read 4-byte authorization token ─────────────────────────── */
    uint8_t tok_buf[UART_PROTO_TOKEN_SIZE];
    for (size_t t = 0; t < UART_PROTO_TOKEN_SIZE; t++) {
        if (read_byte_timeout(uart_dev, &tok_buf[t], UART_PROTO_RX_TIMEOUT_MS) != 0) {
            printk("[PROTO] Timeout reading security token byte %u\n", (unsigned)t);
            return UART_PROTO_ERR_TIMEOUT;
        }
    }
    uint32_t received_token = ((uint32_t)tok_buf[0] << 24) |
                              ((uint32_t)tok_buf[1] << 16) |
                              ((uint32_t)tok_buf[2] << 8)  |
                              (uint32_t)tok_buf[3];

    if (received_token != SCRIPT_AUTH_TOKEN) {
        printk("[SECURITY ERROR] Unauthorized frame / invalid token: 0x%08X (expected 0x%08X)\n",
               (unsigned)received_token, (unsigned)SCRIPT_AUTH_TOKEN);
        return UART_PROTO_ERR_AUTH;
    }

    /* ── Step 4: Read payload bytes ──────────────────────────────────────── */
    printk("[PROTO] Receiving %u byte payload (token 0x%08X verified)...\n",
           (unsigned)payload_len, (unsigned)received_token);
    for (uint16_t i = 0; i < payload_len; i++) {
        if (read_byte_timeout(uart_dev, (uint8_t *)&out_buf[i],
                              UART_PROTO_RX_TIMEOUT_MS) != 0) {
            printk("[PROTO] Timeout reading payload byte %u\n", (unsigned)i);
            return UART_PROTO_ERR_TIMEOUT;
        }
    }
    out_buf[payload_len] = '\0';

    /* ── Step 5: Read 2-byte CRC ─────────────────────────────────────────── */
    uint8_t crc_h, crc_l;
    if (read_byte_timeout(uart_dev, &crc_h, UART_PROTO_RX_TIMEOUT_MS) != 0 ||
        read_byte_timeout(uart_dev, &crc_l, UART_PROTO_RX_TIMEOUT_MS) != 0) {
        printk("[PROTO] Timeout reading CRC bytes\n");
        return UART_PROTO_ERR_TIMEOUT;
    }
    uint16_t received_crc = ((uint16_t)crc_h << 8) | (uint16_t)crc_l;

    /* ── Step 6: Validate CRC over [TOKEN + PAYLOAD] ─────────────────────── */
    uint16_t computed_crc = uart_proto_crc16_update(0xFFFFu, tok_buf, UART_PROTO_TOKEN_SIZE);
    computed_crc = uart_proto_crc16_update(computed_crc, (const uint8_t *)out_buf, payload_len);

    if (computed_crc != received_crc) {
        printk("[PROTO] CRC mismatch! received=0x%04X computed=0x%04X\n",
               (unsigned)received_crc, (unsigned)computed_crc);
        return UART_PROTO_ERR_CRC;
    }

    *out_len = payload_len;
    printk("[PROTO] Frame OK: %u bytes, CRC=0x%04X\n",
           (unsigned)payload_len, (unsigned)computed_crc);
    return UART_PROTO_OK;
}

/* ── Send a framed script ────────────────────────────────────────────────── */
int uart_proto_send_frame(const struct device *uart_dev,
                          const uint8_t *payload, size_t length)
{
    if (uart_dev == NULL || payload == NULL) {
        return UART_PROTO_ERR_DEVICE;
    }
    if (!device_is_ready(uart_dev)) {
        return UART_PROTO_ERR_DEVICE;
    }
    if (length == 0 || length > UART_PROTO_MAX_PAYLOAD) {
        return UART_PROTO_ERR_TOO_LARGE;
    }

    uint8_t tok_buf[UART_PROTO_TOKEN_SIZE] = {
        (uint8_t)((SCRIPT_AUTH_TOKEN >> 24) & 0xFF),
        (uint8_t)((SCRIPT_AUTH_TOKEN >> 16) & 0xFF),
        (uint8_t)((SCRIPT_AUTH_TOKEN >> 8) & 0xFF),
        (uint8_t)(SCRIPT_AUTH_TOKEN & 0xFF),
    };

    /* CRC over [TOKEN + PAYLOAD] */
    uint16_t crc = uart_proto_crc16_update(0xFFFFu, tok_buf, UART_PROTO_TOKEN_SIZE);
    crc = uart_proto_crc16_update(crc, payload, length);

    /* START byte */
    uart_poll_out(uart_dev, UART_PROTO_START_BYTE);

    /* Length big-endian */
    uart_poll_out(uart_dev, (unsigned char)((length >> 8) & 0xFF));
    uart_poll_out(uart_dev, (unsigned char)(length & 0xFF));

    /* Security token 4 bytes */
    for (size_t t = 0; t < UART_PROTO_TOKEN_SIZE; t++) {
        uart_poll_out(uart_dev, tok_buf[t]);
    }

    /* Payload */
    for (size_t i = 0; i < length; i++) {
        uart_poll_out(uart_dev, (unsigned char)payload[i]);
    }

    /* CRC big-endian */
    uart_poll_out(uart_dev, (unsigned char)((crc >> 8) & 0xFF));
    uart_poll_out(uart_dev, (unsigned char)(crc & 0xFF));

    return UART_PROTO_OK;
}
