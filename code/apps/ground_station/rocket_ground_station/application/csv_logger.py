"""Lossless CSV persistence for typed ground-station telemetry.

The logger deliberately accepts application-layer models rather than wire
frames or UI dictionaries.  Optional engineering values therefore retain the
pipeline's validity semantics: an unavailable value is written as an empty
cell, never as a fabricated zero.
"""

from __future__ import annotations

import csv
from collections.abc import Callable, Mapping
from datetime import datetime, timezone
from pathlib import Path
from typing import TextIO

from .models import PipelineDiagnostics, TelemetrySnapshot


METADATA_COLUMNS = (
    "host_utc_time",
    "protocol_version",
    "boot_id",
    "boot_session",
    "sequence",
    "uptime_ms",
    "mission_time_s",
    "valid_flags",
    "source_flags",
)

DIAGNOSTIC_COLUMNS = (
    "diagnostic_total_frames",
    "diagnostic_v2_frames",
    "diagnostic_decode_failures",
    "diagnostic_boot_sessions",
    "diagnostic_boot_changes",
    "diagnostic_sequence_gaps",
    "diagnostic_duplicate_sequences",
    "diagnostic_out_of_order_sequences",
    "diagnostic_parser_crc_failures",
    "diagnostic_parser_resync_count",
    "diagnostic_current_boot_id",
    "diagnostic_last_sequence",
)

ENGINEERING_COLUMNS = (
    "latitude_deg",
    "longitude_deg",
    "gps_altitude_m",
    "gps_speed_mps",
    "satellite_count",
    "roll_deg",
    "pitch_deg",
    "heading_deg",
    "gyro_x_dps",
    "gyro_y_dps",
    "gyro_z_dps",
    "acceleration_x_g",
    "acceleration_y_g",
    "acceleration_z_g",
    "barometric_altitude_m",
    "primary_altitude_m",
    "battery_v",
    "servo_power_v",
    "servo_angle_deg",
    "flight_state",
    "error_code",
    "water_detected",
)

CSV_COLUMNS = METADATA_COLUMNS + DIAGNOSTIC_COLUMNS + ENGINEERING_COLUMNS


def _system_utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _as_utc(value: datetime) -> datetime:
    if not isinstance(value, datetime):
        raise TypeError("clock must return a datetime")
    if value.tzinfo is None or value.utcoffset() is None:
        raise ValueError("clock must return a timezone-aware datetime")
    return value.astimezone(timezone.utc)


def _iso_utc(value: datetime) -> str:
    return (
        _as_utc(value)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def _filename_timestamp(value: datetime) -> str:
    return _as_utc(value).strftime("%Y%m%dT%H%M%S.%fZ")


def _empty_for_none(values: Mapping[str, object]) -> dict[str, object]:
    return {
        key: "" if value is None else value
        for key, value in values.items()
    }


class TelemetryCsvLogger:
    """Write typed telemetry snapshots to a unique CSV file.

    ``output_directory`` has no default on purpose.  Callers must select an
    explicit runtime-data location instead of accidentally writing logs into
    the process working directory.
    """

    def __init__(
        self,
        output_directory: str | Path,
        *,
        filename_prefix: str = "telemetry",
        clock: Callable[[], datetime] = _system_utc_now,
    ) -> None:
        if not filename_prefix:
            raise ValueError("filename_prefix must not be empty")
        if Path(filename_prefix).name != filename_prefix:
            raise ValueError("filename_prefix must not contain a path")

        self._clock = clock
        self._closed = False
        self._output_directory = Path(output_directory)
        self._output_directory.mkdir(parents=True, exist_ok=True)

        timestamp = _filename_timestamp(self._clock())
        self._path, self._file = self._open_unique_file(
            self._output_directory,
            filename_prefix,
            timestamp,
        )
        self._writer = csv.DictWriter(
            self._file,
            fieldnames=CSV_COLUMNS,
            extrasaction="raise",
            lineterminator="\n",
        )
        self._writer.writeheader()

    @staticmethod
    def _open_unique_file(
        output_directory: Path,
        filename_prefix: str,
        timestamp: str,
    ) -> tuple[Path, TextIO]:
        """Atomically reserve a filename, suffixing on timestamp collisions."""

        collision = 0
        while True:
            suffix = "" if collision == 0 else f"_{collision:03d}"
            path = (
                output_directory
                / f"{filename_prefix}_{timestamp}{suffix}.csv"
            )
            try:
                return path, path.open(
                    "x",
                    encoding="utf-8",
                    newline="",
                )
            except FileExistsError:
                collision += 1

    @property
    def path(self) -> Path:
        return self._path

    @property
    def closed(self) -> bool:
        return self._closed

    def log(
        self,
        snapshot: TelemetrySnapshot,
        diagnostics: PipelineDiagnostics,
    ) -> None:
        """Append one snapshot and its contemporaneous pipeline diagnostics."""

        if self._closed:
            raise RuntimeError("cannot log to a closed telemetry CSV")
        if not isinstance(snapshot, TelemetrySnapshot):
            raise TypeError("snapshot must be a TelemetrySnapshot")
        if not isinstance(diagnostics, PipelineDiagnostics):
            raise TypeError("diagnostics must be PipelineDiagnostics")

        row = {
            "host_utc_time": _iso_utc(self._clock()),
            "protocol_version": snapshot.protocol_version,
            "boot_id": snapshot.boot_id,
            "boot_session": snapshot.boot_session,
            "sequence": snapshot.sequence,
            "uptime_ms": snapshot.uptime_ms,
            "mission_time_s": snapshot.mission_time_s,
            "valid_flags": snapshot.valid_flags,
            "source_flags": snapshot.source_flags,
            "diagnostic_total_frames": diagnostics.total_frames,
            "diagnostic_v2_frames": diagnostics.v2_frames,
            "diagnostic_decode_failures": diagnostics.decode_failures,
            "diagnostic_boot_sessions": diagnostics.boot_sessions,
            "diagnostic_boot_changes": diagnostics.boot_changes,
            "diagnostic_sequence_gaps": diagnostics.sequence_gaps,
            "diagnostic_duplicate_sequences": (
                diagnostics.duplicate_sequences
            ),
            "diagnostic_out_of_order_sequences": (
                diagnostics.out_of_order_sequences
            ),
            "diagnostic_parser_crc_failures": (
                diagnostics.parser_crc_failures
            ),
            "diagnostic_parser_resync_count": (
                diagnostics.parser_resync_count
            ),
            "diagnostic_current_boot_id": diagnostics.current_boot_id,
            "diagnostic_last_sequence": diagnostics.last_sequence,
            "latitude_deg": snapshot.latitude_deg,
            "longitude_deg": snapshot.longitude_deg,
            "gps_altitude_m": snapshot.gps_altitude_m,
            "gps_speed_mps": snapshot.gps_speed_mps,
            "satellite_count": snapshot.satellite_count,
            "roll_deg": snapshot.roll_deg,
            "pitch_deg": snapshot.pitch_deg,
            "heading_deg": snapshot.heading_deg,
            "gyro_x_dps": snapshot.gyro_x_dps,
            "gyro_y_dps": snapshot.gyro_y_dps,
            "gyro_z_dps": snapshot.gyro_z_dps,
            "acceleration_x_g": snapshot.acceleration_x_g,
            "acceleration_y_g": snapshot.acceleration_y_g,
            "acceleration_z_g": snapshot.acceleration_z_g,
            "barometric_altitude_m": snapshot.barometric_altitude_m,
            "primary_altitude_m": snapshot.primary_altitude_m,
            "battery_v": snapshot.battery_v,
            "servo_power_v": snapshot.servo_power_v,
            "servo_angle_deg": snapshot.servo_angle_deg,
            "flight_state": snapshot.flight_state,
            "error_code": snapshot.error_code,
            "water_detected": snapshot.water_detected,
        }
        self._writer.writerow(_empty_for_none(row))

    def flush(self) -> None:
        if self._closed:
            raise RuntimeError("cannot flush a closed telemetry CSV")
        self._file.flush()

    def close(self) -> None:
        if self._closed:
            return
        self._file.close()
        self._closed = True

    def __enter__(self) -> TelemetryCsvLogger:
        if self._closed:
            raise RuntimeError("cannot enter a closed telemetry CSV")
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: object,
    ) -> bool:
        self.close()
        return False


# Friendly alternative spelling for callers that prefer CSV as an initialism.
TelemetryCSVLogger = TelemetryCsvLogger


__all__ = [
    "CSV_COLUMNS",
    "DIAGNOSTIC_COLUMNS",
    "ENGINEERING_COLUMNS",
    "METADATA_COLUMNS",
    "TelemetryCSVLogger",
    "TelemetryCsvLogger",
]
