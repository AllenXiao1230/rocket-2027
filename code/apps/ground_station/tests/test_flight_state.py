from pathlib import Path
import sys
import unittest


_APP_ROOT = Path(__file__).resolve().parents[1]
_REPO_ROOT = next(
    parent
    for parent in Path(__file__).resolve().parents
    if (parent / "protocol" / "python" / "rocket_protocol").is_dir()
)
sys.path.insert(0, str(_APP_ROOT))
sys.path.insert(0, str(_REPO_ROOT / "protocol" / "python"))

from rocket_ground_station.application import (  # noqa: E402
    ErrorBit,
    FlightState,
    coerce_flight_state,
    decode_error,
    decode_state,
)


class FlightStateTests(unittest.TestCase):
    def test_known_and_unknown_states(self):
        self.assertEqual(coerce_flight_state(3), FlightState.ASCENT)
        self.assertIsNone(coerce_flight_state(255))
        self.assertEqual(decode_state(4), "過頂點")
        self.assertEqual(decode_state(None), "UNKNOWN")

    def test_error_mask_preserves_unknown_bits(self):
        self.assertEqual(decode_error(None), "UNKNOWN")
        self.assertEqual(decode_error(0), "OK")
        self.assertEqual(
            decode_error(int(ErrorBit.SD_INIT | ErrorBit.GPS_NO_FIX)),
            "SD | GPS",
        )
        self.assertEqual(decode_error(1 << 9), "0x200")


if __name__ == "__main__":
    unittest.main()
