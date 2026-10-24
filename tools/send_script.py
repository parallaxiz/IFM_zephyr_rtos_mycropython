#!/usr/bin/env python3
"""
tools/send_script.py — PC-side script sender for the Zephyr MicroPython UART framing protocol.

Reads a .py file, wraps it in the framed protocol (START | LEN | PAYLOAD | CRC16),
and transmits it over the serial port to the nRF5340 DK.

Frame format:
    [0xAA] [LEN_HIGH] [LEN_LOW] [PAYLOAD ...] [CRC16_HIGH] [CRC16_LOW]

Usage:
    python3 tools/send_script.py <port> <script.py> [--baud BAUD]

Examples:
    python3 tools/send_script.py /dev/ttyACM0 script.py
    python3 tools/send_script.py /dev/ttyACM0 script.py --baud 115200
    python3 tools/send_script.py COM3 script.py          # Windows

Requirements:
    pip install pyserial
"""

import sys
import argparse
import struct
import time

try:
    import serial
except ImportError:
    serial = None

# ── Protocol constants (must match uart_protocol.h) ──────────────────────────
PROTO_START_BYTE    = 0xAA
PROTO_MAX_PAYLOAD   = 6000
SCRIPT_AUTH_TOKEN   = 0xDEADBEEF
PROTO_CRC_POLY      = 0x1021
PROTO_CRC_INIT      = 0xFFFF


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT: poly=0x1021, init=0xFFFF — matches uart_proto_crc16() in C."""
    crc = PROTO_CRC_INIT
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ PROTO_CRC_POLY
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc


def build_frame(payload: bytes, token: int = SCRIPT_AUTH_TOKEN) -> bytes:
    """
    Construct a complete protocol frame with authorization token:
    Returns: START(1) + LEN(2) + TOKEN(4) + PAYLOAD(N) + CRC(2)
    CRC16 is calculated over [TOKEN + PAYLOAD].
    """
    if len(payload) == 0 or len(payload) > PROTO_MAX_PAYLOAD:
        raise ValueError(f"Payload size {len(payload)} out of range (1..{PROTO_MAX_PAYLOAD})")

    token_bytes = struct.pack(">I", token)
    data_to_crc = token_bytes + payload
    crc = crc16_ccitt(data_to_crc)

    frame = bytes([PROTO_START_BYTE])
    frame += struct.pack(">H", len(payload))   # big-endian 16-bit payload length
    frame += token_bytes                       # 4-byte auth token
    frame += payload                           # script payload
    frame += struct.pack(">H", crc)            # big-endian 16-bit CRC
    return frame


def send_script(port: str, script_path: str, baud: int = 115200) -> None:
    """Read script file, build frame, send over serial, and print board response."""

    # ── Read script ───────────────────────────────────────────────────────────
    try:
        with open(script_path, 'rb') as f:
            payload = f.read()
    except FileNotFoundError:
        print(f"ERROR: Script file not found: {script_path}")
        sys.exit(1)

    if len(payload) == 0:
        print("ERROR: Script file is empty.")
        sys.exit(1)

    if len(payload) > PROTO_MAX_PAYLOAD:
        print(f"ERROR: Script is {len(payload)} bytes — exceeds protocol maximum of {PROTO_MAX_PAYLOAD} bytes.")
        sys.exit(1)

    # ── Build frame ───────────────────────────────────────────────────────────
    frame = build_frame(payload)
    crc   = struct.unpack(">H", frame[-2:])[0]

    print(f"Script   : {script_path}")
    print(f"Payload  : {len(payload)} bytes")
    print(f"CRC16    : 0x{crc:04X}")
    print(f"Frame    : {len(frame)} bytes total")
    print(f"Port     : {port} @ {baud} baud")
    print("-" * 50)

    # ── Open serial port ──────────────────────────────────────────────────────
    if serial is None:
        print("ERROR: pyserial is not installed. Please run: pip install pyserial")
        sys.exit(1)

    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.2)
    except serial.SerialException as e:
        print(f"ERROR: Could not open serial port {port}: {e}")
        sys.exit(1)

    try:
        # Allow board time to be ready after any previous reset
        time.sleep(0.5)

        # Flush any stale data
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # ── Transmit frame ────────────────────────────────────────────────────
        print("Sending frame...")
        ser.write(frame)
        ser.flush()
        print("Frame sent. Waiting for board response...\n")

        # Terminal status indicators from firmware
        terminal_responses = [
            "ACK:EXEC_OK",
            "NACK:EXEC_ERR",
            "NACK:CRC",
            "NACK:AUTH",
            "NACK:SIZE",
            "NACK:SAFETY",
            "NACK:TIMEOUT",
            "NACK:ERR",
            "ACK:PING",
        ]

        # ── Read and print board output until idle ────────────────────────────
        idle_count = 0
        while idle_count < 30:   # stop after ~3 seconds of silence
            line = ser.readline()
            if line:
                text = line.decode('utf-8', errors='replace')
                print(text, end='', flush=True)
                idle_count = 0
                if any(resp in text for resp in terminal_responses):
                    break
            else:
                idle_count += 1
                time.sleep(0.1)
    except serial.SerialException as e:
        print(f"\nSerial connection error: {e}")
    finally:
        ser.close()

    print("\nDone.")


def main():
    parser = argparse.ArgumentParser(
        description="Send a Python script to the nRF5340 board using the UART framing protocol."
    )
    parser.add_argument("port",   help="Serial port (e.g. /dev/ttyACM0 or COM3)")
    parser.add_argument("script", help="Path to the Python script file to send")
    parser.add_argument("--baud", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    args = parser.parse_args()

    send_script(args.port, args.script, args.baud)


if __name__ == "__main__":
    main()
