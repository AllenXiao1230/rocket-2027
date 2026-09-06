import csv
from dataclasses import replace
from datetime import datetime, timezone
from pathlib import Path
import tempfile
import unittest

from rocket_ground_station.application.csv_logger import (
    CSV_COLUMNS,
    TelemetryCsvLogger,
)
from rocket_ground_station.application.models import (
    PipelineDiagnostics,
    TelemetrySnapshot,
)


FIXED_UTC = datetime(
    2027,
    5,
    4,
    3,
    2,
    1,
    123_456,
    tzinfo=timezone.utc,
)


def snapshot(**changes: object) -> TelemetrySnapshot:
    base = TelemetrySnapshot(
        protocol_version=2,
        source_flags=5,
        boot_id=42,
        sequence=9,
        uptime_ms=12_345,
        valid_flags=0x1234,
        boot_session=3,
        mission_time_s=8.5,
        latitude_deg=25.0123456,
        longitude_deg=121.9876543,
        gps_altitude_m=101.2,
        gps_speed_mps=15.5,
        satellite_count=8,
        roll_deg=1.25,
        pitch_deg=-2.5,
        heading_deg=270.0,
        gyro_x_dps=0.1,
        gyro_y_dps=0.2,
        gyro_z_dps=0.3,
        acceleration_x_g=0.01,
        acceleration_y_g=0.02,
        acceleration_z_g=1.03,
        barometric_altitude_m=99.8,
        battery_v=11.85,
        servo_power_v=5.0,
        servo_angle_deg=90.0,
        flight_state=3,
        error_code=0,
        water_detected=False,
        primary_altitude_m=99.8,
    )
    return replace(base, **changes)


def diagnostics() -> PipelineDiagnostics:
    return PipelineDiagnostics(
        total_frames=20,
        v2_frames=20,
        decode_failures=1,
        boot_sessions=3,
        boot_changes=2,
        sequence_gaps=4,
        duplicate_sequences=5,
        out_of_order_sequences=6,
        parser_crc_failures=7,
        parser_resync_count=8,
        current_boot_id=42,
        last_sequence=9,
    )


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file))


class TelemetryCsvLoggerTests(unittest.TestCase):
    def test_requires_explicit_output_directory(self):
        with self.assertRaises(TypeError):
            TelemetryCsvLogger()  # type: ignore[call-arg]

    def test_creates_directory_and_writes_fixed_metadata(self):
        with tempfile.TemporaryDirectory() as root:
            output_directory = Path(root) / "data" / "runs"
            logger = TelemetryCsvLogger(
                output_directory,
                clock=lambda: FIXED_UTC,
            )
            logger.log(snapshot(), diagnostics())
            logger.flush()
            path = logger.path
            logger.close()

            self.assertTrue(output_directory.is_dir())
            self.assertEqual(
                path.name,
                "telemetry_20270504T030201.123456Z.csv",
            )
            rows = read_rows(path)
            self.assertEqual(len(rows), 1)
            self.assertEqual(tuple(rows[0]), CSV_COLUMNS)
            self.assertEqual(rows[0]["host_utc_time"], "2027-05-04T03:02:01.123Z")
            self.assertEqual(rows[0]["protocol_version"], "2")
            self.assertEqual(rows[0]["boot_id"], "42")
            self.assertEqual(rows[0]["boot_session"], "3")
            self.assertEqual(rows[0]["sequence"], "9")
            self.assertEqual(rows[0]["valid_flags"], str(0x1234))
            self.assertEqual(rows[0]["source_flags"], "5")
            self.assertEqual(rows[0]["diagnostic_sequence_gaps"], "4")
            self.assertEqual(rows[0]["diagnostic_duplicate_sequences"], "5")
            self.assertEqual(
                rows[0]["diagnostic_out_of_order_sequences"],
                "6",
            )
            self.assertEqual(rows[0]["diagnostic_parser_crc_failures"], "7")
            self.assertEqual(rows[0]["diagnostic_parser_resync_count"], "8")

    def test_none_engineering_and_metadata_values_are_empty(self):
        with tempfile.TemporaryDirectory() as root:
            with TelemetryCsvLogger(
                root,
                clock=lambda: FIXED_UTC,
            ) as logger:
                logger.log(
                    snapshot(
                        boot_id=None,
                        sequence=None,
                        valid_flags=None,
                        boot_session=None,
                        servo_power_v=None,
                        servo_angle_deg=None,
                        water_detected=None,
                        primary_altitude_m=None,
                    ),
                    replace(
                        diagnostics(),
                        current_boot_id=None,
                        last_sequence=None,
                    ),
                )
                path = logger.path

            row = read_rows(path)[0]
            for column in (
                "boot_id",
                "boot_session",
                "sequence",
                "valid_flags",
                "diagnostic_current_boot_id",
                "diagnostic_last_sequence",
                "servo_power_v",
                "servo_angle_deg",
                "water_detected",
                "primary_altitude_m",
            ):
                self.assertEqual(row[column], "", column)
            self.assertNotEqual(row["servo_power_v"], "0")
            self.assertNotEqual(row["water_detected"], "0")

    def test_same_timestamp_uses_unique_files_without_overwrite(self):
        with tempfile.TemporaryDirectory() as root:
            first = TelemetryCsvLogger(root, clock=lambda: FIXED_UTC)
            first.log(snapshot(sequence=1), diagnostics())
            first_path = first.path
            first.close()

            second = TelemetryCsvLogger(root, clock=lambda: FIXED_UTC)
            second.log(snapshot(sequence=2), diagnostics())
            second_path = second.path
            second.close()

            self.assertNotEqual(first_path, second_path)
            self.assertTrue(first_path.exists())
            self.assertTrue(second_path.exists())
            self.assertEqual(read_rows(first_path)[0]["sequence"], "1")
            self.assertEqual(read_rows(second_path)[0]["sequence"], "2")
            self.assertTrue(second_path.stem.endswith("_001"))

    def test_context_manager_closes_and_close_is_idempotent(self):
        with tempfile.TemporaryDirectory() as root:
            with TelemetryCsvLogger(
                root,
                clock=lambda: FIXED_UTC,
            ) as logger:
                logger.log(snapshot(), diagnostics())
                self.assertFalse(logger.closed)
                path = logger.path

            self.assertTrue(logger.closed)
            logger.close()
            self.assertEqual(len(read_rows(path)), 1)
            with self.assertRaises(RuntimeError):
                logger.log(snapshot(), diagnostics())
            with self.assertRaises(RuntimeError):
                logger.flush()

    def test_write_flush_and_close_errors_are_not_swallowed(self):
        class FailingWriter:
            def writerow(self, row: object) -> None:
                raise OSError("write failed")

        class FailingFile:
            def flush(self) -> None:
                raise OSError("flush failed")

            def close(self) -> None:
                raise OSError("close failed")

        with tempfile.TemporaryDirectory() as root:
            writer_logger = TelemetryCsvLogger(
                root,
                filename_prefix="writer",
                clock=lambda: FIXED_UTC,
            )
            writer_logger._writer = FailingWriter()  # type: ignore[assignment]
            with self.assertRaisesRegex(OSError, "write failed"):
                writer_logger.log(snapshot(), diagnostics())
            writer_logger.close()

            file_logger = TelemetryCsvLogger(
                root,
                filename_prefix="file",
                clock=lambda: FIXED_UTC,
            )
            real_file = file_logger._file
            real_file.close()
            file_logger._file = FailingFile()  # type: ignore[assignment]
            with self.assertRaisesRegex(OSError, "flush failed"):
                file_logger.flush()
            with self.assertRaisesRegex(OSError, "close failed"):
                file_logger.close()
            self.assertFalse(file_logger.closed)


if __name__ == "__main__":
    unittest.main()
