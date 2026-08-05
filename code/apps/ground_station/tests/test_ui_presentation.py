from pathlib import Path
import sys
import unittest


_APP_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_APP_ROOT))

from rocket_ground_station.application import FlightState  # noqa: E402
from rocket_ground_station.ui.presentation import (  # noqa: E402
    UNKNOWN_STATE_TEXT,
    UNAVAILABLE_TEXT,
    flight_state_name,
    normalized_flight_state,
    speed_altitude_texts,
)


class UiPresentationTests(unittest.TestCase):
    def test_missing_and_invalid_states_are_unknown(self):
        for value in (None, 255, -1, "ASCENT", object()):
            with self.subTest(value=value):
                self.assertIsNone(normalized_flight_state(value))
                self.assertEqual(flight_state_name(value), UNKNOWN_STATE_TEXT)

    def test_known_state_uses_enum_name(self):
        self.assertEqual(
            normalized_flight_state(FlightState.ASCENT),
            FlightState.ASCENT,
        )
        self.assertEqual(flight_state_name(FlightState.ASCENT), "ASCENT")

    def test_missing_speed_and_altitude_render_unavailable(self):
        self.assertEqual(
            speed_altitude_texts(None, None, None, None),
            (
                UNAVAILABLE_TEXT,
                UNAVAILABLE_TEXT,
                UNAVAILABLE_TEXT,
                UNAVAILABLE_TEXT,
            ),
        )

    def test_real_zero_measurements_are_not_treated_as_missing(self):
        self.assertEqual(
            speed_altitude_texts(0.0, 0, 0.0, 0),
            ("0.00", "0.00", "0.0", "0.0"),
        )

    def test_channel_specific_values_never_cross_fill(self):
        self.assertEqual(
            speed_altitude_texts(12.5, None, 123.4, None),
            ("12.50", UNAVAILABLE_TEXT, "123.4", UNAVAILABLE_TEXT),
        )
        self.assertEqual(
            speed_altitude_texts(None, 3.5, None, 456.7),
            (UNAVAILABLE_TEXT, "3.50", UNAVAILABLE_TEXT, "456.7"),
        )


if __name__ == "__main__":
    unittest.main()
