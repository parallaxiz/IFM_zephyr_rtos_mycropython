/**
 * @file uart_protocol.h
 * @brief Framed UART protocol with CRC16 for reliable script transmission.
 *
 * Frame format:
 *   [START: 0xAA] [LEN_HIGH] [LEN_LOW] [PAYLOAD: N bytes] [CRC16_HIGH] [CRC16_LOW]
 *
 * - START byte   : 0xAA (1 byte)
 * - LEN          : 16-bit big-endian payload length (2 bytes)
 * - PAYLOAD      : Python script source bytes (up to UART_PROTO_MAX_PAYLOAD bytes)
 * - CRC16        : CRC-16/CCITT over PAYLOAD only, big-endian (2 bytes)
 *
 * Total overhead per frame: 5 bytes (1 start + 2 len + 2 crc)
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Frame constants ─────────────────────────────────────────────────────── */
#define UART_PROTO_START_BYTE       0xAAu
#define UART_PROTO_HEADER_SIZE      3u   /* START(1) + LEN_HIGH(1) + LEN_LOW(1) */
#define UART_PROTO_CRC_SIZE         2u
#define UART_PROTO_OVERHEAD         (UART_PROTO_HEADER_SIZE + UART_PROTO_CRC_SIZE)
#define UART_PROTO_MAX_PAYLOAD      6000u
#define UART_PROTO_MAX_SCRIPT_LEN   UART_PROTO_MAX_PAYLOAD
#define UART_PROTO_MAX_FRAME_SIZE   (UART_PROTO_MAX_PAYLOAD + UART_PROTO_OVERHEAD)

/* Receive timeout waiting for the full frame (ms) */
#define UART_PROTO_RX_TIMEOUT_MS    5000u

/* ── Return codes ────────────────────────────────────────────────────────── */
#define UART_PROTO_OK               0
#define UART_PROTO_ERR_TIMEOUT     -1
#define UART_PROTO_ERR_BAD_START   -2
#define UART_PROTO_ERR_TOO_LARGE   -3
#define UART_PROTO_ERR_CRC         -4
#define UART_PROTO_ERR_DEVICE      -5

/**
 * @brief Compute CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) over a buffer.
 *
 * @param data   Pointer to data buffer.
 * @param length Number of bytes.
 * @return       16-bit CRC value.
 */
uint16_t uart_proto_crc16(const uint8_t *data, size_t length);

/**
 * @brief Receive one complete framed script over UART.
 *
 * Blocks until a valid frame is received or the timeout expires.
 * Validates the CRC before copying payload into out_buf.
 *
 * @param uart_dev   Zephyr UART device pointer.
 * @param out_buf    Buffer to write the received Python script into.
 * @param buf_size   Size of out_buf (must be >= UART_PROTO_MAX_PAYLOAD + 1).
 * @param out_len    Set to the number of bytes written into out_buf on success.
 * @return           UART_PROTO_OK on success, negative error code otherwise.
 */
int uart_proto_receive_frame(const struct device *uart_dev,
                             char *out_buf, size_t buf_size,
                             size_t *out_len);

/**
 * @brief Send a framed script over UART (for testing / echo use).
 *
 * Builds and transmits: START | LEN_H | LEN_L | PAYLOAD | CRC_H | CRC_L
 *
 * @param uart_dev   Zephyr UART device pointer.
 * @param payload    Python script bytes to send.
 * @param length     Number of bytes in payload.
 * @return           UART_PROTO_OK on success, negative error code otherwise.
 */
int uart_proto_send_frame(const struct device *uart_dev,
                          const uint8_t *payload, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
