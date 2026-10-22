import struct
import unittest

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

def build_frame(payload: bytes) -> bytes:
    if len(payload) == 0 or len(payload) > 6000:
        raise ValueError("Invalid payload length")
    crc = crc16_ccitt(payload)
    return struct.pack(f">BH{len(payload)}sH", 0xAA, len(payload), payload, crc)

def parse_frame(frame: bytes):
    if len(frame) < 5:
        return -1, "Too short"
    start, length = struct.unpack_from(">BH", frame, 0)
    if start != 0xAA:
        return -2, "Bad start byte"
    if len(frame) < 3 + length + 2:
        return -1, "Truncated frame"
    payload = frame[3:3+length]
    received_crc = struct.unpack_from(">H", frame, 3 + length)[0]
    computed_crc = crc16_ccitt(payload)
    if received_crc != computed_crc:
        return -4, "CRC mismatch"
    return 0, payload

class TestProtocol(unittest.TestCase):
    def test_crc16_standard_vector(self):
        # Standard CCITT check value for "123456789" with init=0xFFFF, poly=0x1021 is 0x29B1
        data = b"123456789"
        self.assertEqual(crc16_ccitt(data), 0x29B1)

    def test_valid_frame(self):
        script = b"print('Hello, Zephyr!')\n"
        frame = build_frame(script)
        status, payload = parse_frame(frame)
        self.assertEqual(status, 0)
        self.assertEqual(payload, script)

    def test_corrupted_crc(self):
        script = b"a = 10 + 20"
        frame = bytearray(build_frame(script))
        # Corrupt one payload byte
        frame[5] ^= 0xFF
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
