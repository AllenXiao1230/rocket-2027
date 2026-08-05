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

from rocket_protocol import (  # noqa: E402
    INVALID_I16,
    INVALID_I32,
    INVALID_U16,
    INVALID_U8,
    VALID_ACCELERATION,
    VALID_ATTITUDE,
    VALID_BARO_ALTITUDE,
    VALID_BATTERY,
    VALID_ERROR_CODE,
    VALID_FLIGHT_STATE,
    VALID_GPS_ALTITUDE,
    VALID_GPS_POSITION,
    VALID_GPS_SPEED,
    VALID_GYRO,
    VALID_HEADING,
    VALID_SATELLITE_COUNT,
    VALID_SERVO_ANGLE,
    VALID_SERVO_POWER,
    VALID_WATER,
    build_v1_frame,
    build_v2_frame,
)
from rocket_ground_station.application import TelemetryPipeline  # noqa: E402


ALL_VALID = (
    VALID_GPS_POSITION
    | VALID_GPS_ALTITUDE
    | VALID_GPS_SPEED
    | VALID_SATELLITE_COUNT
    | VALID_ATTITUDE
    | VALID_HEADING
    | VALID_GYRO
    | VALID_ACCELERATION
    | VALID_BARO_ALTITUDE
    | VALID_BATTERY
    | VALID_SERVO_POWER
    | VALID_SERVO_ANGLE
    | VALID_FLIGHT_STATE
    | VALID_ERROR_CODE
    | VALID_WATER
)


class TelemetryPipelineTests(unittest.TestCase):
    def test_v2_snapshot_converts_only_valid_fields(self):
        flags = (
            VALID_GPS_POSITION
            | VALID_GPS_ALTITUDE
            | VALID_BARO_ALTITUDE
            | VALID_BATTERY
            | VALID_FLIGHT_STATE
            | VALID_ERROR_CODE
            | VALID_WATER
        )
        frame = build_v2_frame(
            source_flags=5,
            boot_id=42,
            sequence=7,
            uptime_ms=12_345,
            valid_flags=flags,
            lat_raw=250_123_456,
            lon_raw=1_219_876_543,
            gps_alt_dm=1234,
            baro_alt_dm=4321,
            battery_mv=11_850,
            flight_state=3,
            error_code=0,
            water_detected=1,
            # These non-sentinel arguments must still be hidden because their
            # corresponding validity bits are clear.
            gps_speed_dms=99,
            roll_cdeg=123,
        )

        pipeline = TelemetryPipeline()
        snapshots = pipeline.feed(frame)

        self.assertEqual(len(snapshots), 1)
        sample = snapshots[0]
        self.assertEqual(sample.protocol_version, 2)
        self.assertEqual(sample.boot_id, 42)
        self.assertEqual(sample.sequence, 7)
        self.assertEqual(sample.boot_session, 1)
        self.assertAlmostEqual(sample.uptime_s, 12.345)
        self.assertAlmostEqual(sample.latitude_deg, 25.0123456)
        self.assertAlmostEqual(sample.longitude_deg, 121.9876543)
        self.assertAlmostEqual(sample.gps_altitude_m, 123.4)
        self.assertAlmostEqual(sample.barometric_altitude_m, 432.1)
        self.assertAlmostEqual(sample.primary_altitude_m, 432.1)
        self.assertAlmostEqual(sample.battery_v, 11.85)
        self.assertEqual(sample.flight_state, 3)
        self.assertEqual(sample.error_code, 0)
        self.assertIs(sample.water_detected, True)
        self.assertIsNone(sample.gps_speed_mps)
        self.assertIsNone(sample.roll_deg)

    def test_clear_valid_flags_never_expose_sentinels(self):
        sample = TelemetryPipeline().feed(
            build_v2_frame(
                boot_id=1,
                sequence=0,
                valid_flags=0,
            )
        )[0]

        optional_values = (
            sample.latitude_deg,
            sample.longitude_deg,
            sample.gps_altitude_m,
            sample.gps_speed_mps,
            sample.satellite_count,
            sample.roll_deg,
            sample.pitch_deg,
            sample.heading_deg,
            sample.gyro_x_dps,
            sample.gyro_y_dps,
            sample.gyro_z_dps,
            sample.acceleration_x_g,
            sample.acceleration_y_g,
            sample.acceleration_z_g,
            sample.barometric_altitude_m,
            sample.battery_v,
            sample.servo_power_v,
            sample.servo_angle_deg,
            sample.flight_state,
            sample.error_code,
            sample.water_detected,
            sample.primary_altitude_m,
        )
        self.assertTrue(all(value is None for value in optional_values))
        self.assertEqual(sample.valid_flags, 0)

    def test_flagged_reserved_sentinel_is_still_none(self):
        # build_v2_frame permits a validity bit while selecting its default
        # sentinel, which verifies the application-side second line of defence.
        sample = TelemetryPipeline().feed(
            build_v2_frame(
                boot_id=1,
                sequence=1,
                valid_flags=ALL_VALID,
                lat_raw=INVALID_I32,
                lon_raw=INVALID_I32,
                gps_alt_dm=INVALID_I16,
                gps_speed_dms=INVALID_I16,
                sat_count=INVALID_U8,
                roll_cdeg=INVALID_I16,
                pitch_cdeg=INVALID_I16,
                yaw_ddeg=INVALID_U16,
                gyro_x_ddeg_s=INVALID_I16,
                gyro_y_ddeg_s=INVALID_I16,
                gyro_z_ddeg_s=INVALID_I16,
                accx_cg=INVALID_I16,
                accy_cg=INVALID_I16,
                accz_cg=INVALID_I16,
                baro_alt_dm=INVALID_I16,
                battery_mv=INVALID_U16,
                servo_power_mv=INVALID_U16,
                servo_angle_ddeg=INVALID_I16,
                flight_state=INVALID_U8,
                error_code=INVALID_U8,
                water_detected=INVALID_U8,
            )
        )[0]

        self.assertIsNone(sample.latitude_deg)
        self.assertIsNone(sample.heading_deg)
        self.assertIsNone(sample.battery_v)
        self.assertIsNone(sample.flight_state)
        self.assertIsNone(sample.water_detected)
        self.assertEqual(sample.error_code, 0xFF)

    def test_full_error_mask_is_valid_in_v2_and_v1(self):
        v2 = TelemetryPipeline().feed(
            build_v2_frame(
                boot_id=1,
                sequence=1,
                valid_flags=VALID_ERROR_CODE,
                error_code=0xFF,
            )
        )[0]
        v1 = TelemetryPipeline().feed(
            build_v1_frame(error_code=0xFF)
        )[0]

        self.assertEqual(v2.error_code, 0xFF)
        self.assertEqual(v1.error_code, 0xFF)

    def test_v1_is_supported_and_sentinels_are_hidden(self):
        frame = build_v1_frame(
            time_ds=25,
            lat_raw=250_000_000,
            lon_raw=1_210_000_000,
            gps_alt_dm=321,
            baro_alt_dm=INVALID_I16,
            battery_mv=12_000,
            flight_state=2,
        )
        sample = TelemetryPipeline().feed(frame)[0]

        self.assertEqual(sample.protocol_version, 1)
        self.assertIsNone(sample.boot_id)
        self.assertIsNone(sample.sequence)
        self.assertIsNone(sample.boot_session)
        self.assertAlmostEqual(sample.uptime_s, 2.5)
        self.assertAlmostEqual(sample.gps_altitude_m, 32.1)
        self.assertIsNone(sample.barometric_altitude_m)
        self.assertAlmostEqual(sample.primary_altitude_m, 32.1)
        self.assertAlmostEqual(sample.battery_v, 12.0)

    def test_sequence_and_boot_diagnostics(self):
        pipeline = TelemetryPipeline()
        frames = b"".join(
            [
                build_v2_frame(boot_id=10, sequence=100),
                build_v2_frame(boot_id=10, sequence=103),
                build_v2_frame(boot_id=10, sequence=103),
                build_v2_frame(boot_id=10, sequence=102),
                build_v2_frame(boot_id=11, sequence=0),
                build_v1_frame(),
            ]
        )

        snapshots = pipeline.feed(frames)
        diagnostics = pipeline.diagnostics

        self.assertEqual(len(snapshots), 4)
        self.assertEqual([sample.sequence for sample in snapshots], [100, 103, 0, None])
        self.assertEqual(diagnostics.total_frames, 6)
        self.assertEqual(diagnostics.v1_frames, 1)
        self.assertEqual(diagnostics.v2_frames, 5)
        self.assertEqual(diagnostics.boot_sessions, 2)
        self.assertEqual(diagnostics.boot_changes, 1)
        self.assertEqual(diagnostics.sequence_gaps, 2)
        self.assertEqual(diagnostics.duplicate_sequences, 1)
        self.assertEqual(diagnostics.out_of_order_sequences, 1)
        self.assertEqual(diagnostics.current_boot_id, 11)
        self.assertEqual(diagnostics.last_sequence, 0)

    def test_stale_sequences_do_not_emit_or_rewind_mission_clock(self):
        flags = VALID_FLIGHT_STATE
        pipeline = TelemetryPipeline()
        frames = b"".join(
            [
                build_v2_frame(
                    boot_id=7,
                    sequence=10,
                    uptime_ms=10_000,
                    valid_flags=flags,
                    flight_state=3,
                ),
                build_v2_frame(
                    boot_id=7,
                    sequence=11,
                    uptime_ms=11_000,
                    valid_flags=flags,
                    flight_state=3,
                ),
                # If emitted, these stale samples would rewind uptime and reset
                # the mission clock by returning the vehicle to PREFLIGHT.
                build_v2_frame(
                    boot_id=7,
                    sequence=11,
                    uptime_ms=1_000,
                    valid_flags=flags,
                    flight_state=2,
                ),
                build_v2_frame(
                    boot_id=7,
                    sequence=9,
                    uptime_ms=2_000,
                    valid_flags=flags,
                    flight_state=2,
                ),
                build_v2_frame(
                    boot_id=7,
                    sequence=12,
                    uptime_ms=12_000,
                    valid_flags=flags,
                    flight_state=3,
                ),
            ]
        )

        snapshots = pipeline.feed(frames)

        self.assertEqual([sample.sequence for sample in snapshots], [10, 11, 12])
        self.assertEqual(
            [sample.mission_time_s for sample in snapshots], [0.0, 1.0, 2.0]
        )
        self.assertEqual(pipeline.diagnostics.duplicate_sequences, 1)
        self.assertEqual(pipeline.diagnostics.out_of_order_sequences, 1)
        self.assertEqual(pipeline.diagnostics.last_sequence, 12)

    def test_sequence_wrap_is_in_order(self):
        pipeline = TelemetryPipeline()
        pipeline.feed(build_v2_frame(boot_id=7, sequence=0xFFFFFFFF))
        pipeline.feed(build_v2_frame(boot_id=7, sequence=0))

        diagnostics = pipeline.diagnostics
        self.assertEqual(diagnostics.sequence_gaps, 0)
        self.assertEqual(diagnostics.out_of_order_sequences, 0)
        self.assertEqual(diagnostics.last_sequence, 0)

    def test_chunking_and_parser_diagnostics_are_exposed(self):
        good = build_v2_frame(boot_id=1, sequence=5)
        bad = bytearray(build_v1_frame())
        bad[-1] ^= 0xFF
        pipeline = TelemetryPipeline()

        self.assertEqual(pipeline.feed(b"\x00garbage" + bytes(bad) + good[:9]), [])
        samples = pipeline.feed(good[9:])

        self.assertEqual(len(samples), 1)
        self.assertGreaterEqual(pipeline.diagnostics.parser_crc_failures, 1)
        self.assertGreaterEqual(pipeline.diagnostics.parser_resync_count, 1)

    def test_reset_clears_all_diagnostics(self):
        pipeline = TelemetryPipeline()
        pipeline.feed(build_v2_frame(boot_id=1, sequence=1))
        pipeline.reset()

        self.assertEqual(pipeline.diagnostics.total_frames, 0)
        self.assertEqual(pipeline.diagnostics.boot_sessions, 0)
        self.assertEqual(pipeline.diagnostics.parser_crc_failures, 0)


if __name__ == "__main__":
    unittest.main()
