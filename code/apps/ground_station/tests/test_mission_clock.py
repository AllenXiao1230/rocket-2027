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

from rocket_ground_station.application import FlightState, MissionClock  # noqa: E402


class MissionClockTests(unittest.TestCase):
    def test_starts_at_first_flight_phase_and_advances(self):
        clock = MissionClock()

        self.assertEqual(clock.update(FlightState.PREFLIGHT, 10.0), 0.0)
        self.assertEqual(clock.update(FlightState.ASCENT, 12.5), 0.0)
        self.assertAlmostEqual(clock.update(FlightState.ASCENT, 14.0), 1.5)
        self.assertAlmostEqual(clock.update(FlightState.APOGEE, 17.0), 4.5)

    def test_joining_during_descent_starts_local_t_plus_zero(self):
        clock = MissionClock()

        self.assertEqual(clock.update(FlightState.DESCENT, 150.0), 0.0)
        self.assertAlmostEqual(clock.update(FlightState.LANDED, 153.25), 3.25)

    def test_preflight_and_uptime_reset_start_new_mission(self):
        clock = MissionClock()
        clock.update(FlightState.ASCENT, 30.0)
        self.assertEqual(clock.update(FlightState.ASCENT, 31.0), 1.0)

        self.assertEqual(clock.update(FlightState.PREFLIGHT, 32.0), 0.0)
        self.assertEqual(clock.update(FlightState.ASCENT, 35.0), 0.0)
        self.assertEqual(clock.update(FlightState.ASCENT, 1.0), 0.0)
        self.assertEqual(clock.update(FlightState.ASCENT, 2.5), 1.5)

    def test_invalid_uptime_is_rejected(self):
        clock = MissionClock()

        with self.assertRaises(ValueError):
            clock.update(FlightState.IDLE, -1.0)
        with self.assertRaises(ValueError):
            clock.update(FlightState.IDLE, float("nan"))


if __name__ == "__main__":
    unittest.main()
