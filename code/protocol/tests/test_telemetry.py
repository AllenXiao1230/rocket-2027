import json
from pathlib import Path
import sys
import unittest

PROTOCOL_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROTOCOL_ROOT / "python"))

from rocket_protocol import (
    INVALID_I16,
    INVALID_I32,
    INVALID_U16,
    INVALID_U8,
    TelemetryStreamParser,
    VALID_ERROR_CODE,
    VALID_FLIGHT_STATE,
    VALID_WATER,
    build_v1_frame,
    build_v2_frame,
    crc16_ccitt_false,
    decode_frame,
    is_valid_v2_frame,
)


class TelemetryProtocolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        vectors_path = PROTOCOL_ROOT / "golden_frames" / "vectors.json"
        cls.vectors = json.loads(vectors_path.read_text())

    def test_crc_standard_check(self):
        self.assertEqual(crc16_ccitt_false(b"123456789"), 0x29B1)

    def test_nominal_golden_frame(self):
        vector = self.vectors["telemetry_v2_nominal"]
        frame = build_v2_frame(**vector["fields"])
        self.assertEqual(frame.hex(), vector["expected_hex"])
        self.assertTrue(is_valid_v2_frame(frame))
        decoded = decode_frame(frame)
        self.assertEqual(decoded.protocol_version, 2)
        self.assertEqual(decoded.boot_id, 0xBEEF)
        self.assertEqual(decoded.sequence, 0x12345678)

    def test_all_invalid_fields_use_deterministic_sentinels(self):
        flags = VALID_FLIGHT_STATE | VALID_ERROR_CODE | VALID_WATER
        frame = build_v2_frame(
            valid_flags=flags,
            flight_state=1,
            error_code=0,
            water_detected=0,
        )
        decoded = decode_frame(frame)
        self.assertEqual(decoded.lat_raw, INVALID_I32)
        self.assertEqual(decoded.gps_alt_dm, INVALID_I16)
        self.assertEqual(decoded.battery_mv, INVALID_U16)
        self.assertEqual(decoded.sat_count, INVALID_U8)

    def test_default_frame_does_not_claim_sentinel_fields_are_valid(self):
        decoded = decode_frame(build_v2_frame())

        self.assertEqual(decoded.valid_flags, 0)
        self.assertEqual(decoded.flight_state, INVALID_U8)
        self.assertEqual(decoded.error_code, INVALID_U8)
        self.assertEqual(decoded.water_detected, INVALID_U8)
        self.assertFalse(decoded.valid(VALID_FLIGHT_STATE))
        self.assertFalse(decoded.valid(VALID_ERROR_CODE))
        self.assertFalse(decoded.valid(VALID_WATER))

    def test_mixed_stream_and_every_split_point(self):
        v1 = build_v1_frame(time_ds=12, flight_state=1)
        vector = self.vectors["telemetry_v2_nominal"]
        v2 = build_v2_frame(**vector["fields"])
        stream = b"BOOT:ready\n" + v1 + b"noise" + v2
        for split in range(len(stream) + 1):
            parser = TelemetryStreamParser()
            frames = parser.feed(stream[:split])
            frames += parser.feed(stream[split:])
            self.assertEqual(frames, [v1, v2], split)

    def test_crc_bit_flip_rejected(self):
        vector = self.vectors["telemetry_v2_nominal"]
        frame = bytearray(build_v2_frame(**vector["fields"]))
        for offset in range(len(frame)):
            changed = bytearray(frame)
            changed[offset] ^= 0x01
            self.assertFalse(is_valid_v2_frame(bytes(changed)), offset)


if __name__ == "__main__":
    unittest.main()
