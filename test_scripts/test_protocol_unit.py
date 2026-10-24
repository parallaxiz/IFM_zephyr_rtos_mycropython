import struct
import unittest

SCRIPT_AUTH_TOKEN = 0xDEADBEEF

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc

import sys
from pathlib import Path

# Add repo root to import path for tools.send_script
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from tools.send_script import build_frame as sender_build_frame, SCRIPT_AUTH_TOKEN as SENDER_TOKEN

def build_frame(payload: bytes, token: int = SCRIPT_AUTH_TOKEN) -> bytes:
    if len(payload) > 6000:
        raise ValueError("Invalid payload length")
    token_bytes = struct.pack(">I", token)
    crc = crc16_ccitt(token_bytes + payload)
    return struct.pack(f">BHI{len(payload)}sH", 0xAA, len(payload), token, payload, crc)

def parse_frame_stream(stream: bytes):
    """Simulates uart_proto_receive_frame byte-by-byte stream parser with sync recovery."""
    idx = 0
    # Step 1: scan for start byte
    while idx < len(stream) and stream[idx] != 0xAA:
        idx += 1
    if idx >= len(stream):
        return -1, "No start byte found", b""
    idx += 1

    # Step 2: read length (2 bytes)
    if idx + 2 > len(stream):
        return -1, "Truncated length", b""
    length = struct.unpack_from(">H", stream, idx)[0]
    idx += 2
    if length > 6000:
        return -3, "Payload too large", b""

    # Step 3: read token (4 bytes)
    if idx + 4 > len(stream):
        return -1, "Truncated token", b""
    token = struct.unpack_from(">I", stream, idx)[0]
    idx += 4
    if token != SCRIPT_AUTH_TOKEN:
        return -6, "Unauthorized / invalid token", b""

    # Step 4: read payload
    if idx + length > len(stream):
        return -1, "Truncated payload", b""
    payload = stream[idx:idx+length]
    idx += length

    # Step 5: read CRC (2 bytes)
    if idx + 2 > len(stream):
        return -1, "Truncated CRC", b""
    received_crc = struct.unpack_from(">H", stream, idx)[0]
    idx += 2

    # Step 6: verify CRC
    token_bytes = struct.pack(">I", token)
    computed_crc = crc16_ccitt(token_bytes + payload)
    if received_crc != computed_crc:
        return -4, "CRC mismatch", b""

    remaining = stream[idx:]
    return 0, payload, remaining

def parse_frame(frame: bytes):
    status, payload, _ = parse_frame_stream(frame)
    return status, payload

class TestProtocol(unittest.TestCase):
    def test_crc16_standard_vector(self):
        # Standard CCITT check value for "123456789" with init=0xFFFF, poly=0x1021 is 0x29B1
        data = b"123456789"
        self.assertEqual(crc16_ccitt(data), 0x29B1)

    def test_valid_frame_with_token(self):
        script = b"print('Hello, Zephyr!')\n"
        frame = build_frame(script)
        status, payload = parse_frame(frame)
        self.assertEqual(status, 0)
        self.assertEqual(payload, script)

    def test_sender_build_frame_compatibility(self):
        script = b"print('Compatibility test from send_script.py')\n"
        frame = sender_build_frame(script)
        status, payload = parse_frame(frame)
        self.assertEqual(status, 0)
        self.assertEqual(payload, script)
        self.assertEqual(SENDER_TOKEN, SCRIPT_AUTH_TOKEN)

    def test_ping_frame_zero_payload(self):
        ping_frame = build_frame(b"")
        self.assertEqual(len(ping_frame), 9)  # 1 start + 2 len + 4 token + 2 crc
        status, payload = parse_frame(ping_frame)
        self.assertEqual(status, 0)
        self.assertEqual(payload, b"")

    def test_stream_noise_recovery(self):
        # Line noise / debug garbage preceding valid frame
        noise = b"\x00\xFF\x55[BOOT] Zephyr OS booting...\r\n\x1B[0m"
        script = b"a = 1 + 2"
        frame = noise + build_frame(script)
        status, payload = parse_frame(frame)
        self.assertEqual(status, 0)
        self.assertEqual(payload, script)

    def test_consecutive_multiple_frames(self):
        script1 = b"print('Frame 1')"
        script2 = b"print('Frame 2')"
        stream = build_frame(script1) + build_frame(script2)

        status1, payload1, rem1 = parse_frame_stream(stream)
        self.assertEqual(status1, 0)
        self.assertEqual(payload1, script1)

        status2, payload2, rem2 = parse_frame_stream(rem1)
        self.assertEqual(status2, 0)
        self.assertEqual(payload2, script2)
        self.assertEqual(rem2, b"")

    def test_invalid_security_token(self):
        script = b"print('Malicious Injection')"
        # Send frame with invalid authorization token
        frame = build_frame(script, token=0xCAFEBABE)
        status, msg = parse_frame(frame)
        self.assertEqual(status, -6)
        self.assertIn("Unauthorized", msg)

    def test_corrupted_crc(self):
        script = b"a = 10 + 20"
        frame = bytearray(build_frame(script))
        # Corrupt one payload byte
        frame[9] ^= 0xFF
        status, _ = parse_frame(bytes(frame))
        self.assertEqual(status, -4)

    def test_bad_start_byte(self):
        script = b"x = 1"
        frame = bytearray(build_frame(script))
        frame[0] = 0x55
        status, _ = parse_frame(bytes(frame))
        self.assertEqual(status, -1)  # No start byte found in single packet

    def test_truncated_frame(self):
        script = b"long_script_bytes_exceeding_stream"
        frame = build_frame(script)
        truncated = frame[:len(frame) - 5]
        status, msg = parse_frame(truncated)
        self.assertEqual(status, -1)

    def test_payload_size_limits(self):
        with self.assertRaises(ValueError):
            build_frame(b"x" * 6001)

if __name__ == "__main__":
    unittest.main()
