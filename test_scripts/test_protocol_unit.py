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

def build_frame(payload: bytes, token: int = SCRIPT_AUTH_TOKEN) -> bytes:
    if len(payload) == 0 or len(payload) > 6000:
        raise ValueError("Invalid payload length")
    token_bytes = struct.pack(">I", token)
    crc = crc16_ccitt(token_bytes + payload)
    return struct.pack(f">BHI{len(payload)}sH", 0xAA, len(payload), token, payload, crc)

def parse_frame(frame: bytes):
    # Min size: START(1) + LEN(2) + TOKEN(4) + CRC(2) = 9 bytes
    if len(frame) < 9:
        return -1, "Too short"
    start, length, token = struct.unpack_from(">BHI", frame, 0)
    if start != 0xAA:
        return -2, "Bad start byte"
    if token != SCRIPT_AUTH_TOKEN:
        return -6, "Unauthorized / invalid token"
    if len(frame) < 7 + length + 2:
        return -1, "Truncated frame"
    payload = frame[7:7+length]
    received_crc = struct.unpack_from(">H", frame, 7 + length)[0]
    token_bytes = struct.pack(">I", token)
    computed_crc = crc16_ccitt(token_bytes + payload)
    if received_crc != computed_crc:
        return -4, "CRC mismatch"
    return 0, payload

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
        self.assertEqual(status, -2)

    def test_payload_size_limits(self):
        with self.assertRaises(ValueError):
            build_frame(b"")
        with self.assertRaises(ValueError):
            build_frame(b"x" * 6001)

if __name__ == "__main__":
    unittest.main()
