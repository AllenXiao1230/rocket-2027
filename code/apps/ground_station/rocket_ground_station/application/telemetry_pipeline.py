"""Byte-stream to typed-snapshot telemetry application pipeline."""

from __future__ import annotations

from importlib.util import find_spec
from pathlib import Path
import sys


# During repository-local execution the canonical protocol package is a sibling
# project rather than an installed dependency.  Prefer an installed package,
# then locate that same canonical source tree without importing the legacy
# ground_station.protocol compatibility adapter.
if find_spec("rocket_protocol") is None:
    for parent in Path(__file__).resolve().parents:
        candidate = parent / "protocol" / "python"
        if (candidate / "rocket_protocol").is_dir():
            sys.path.insert(0, str(candidate))
            break

from rocket_protocol import (
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
    FrameDecodeError,
    TelemetryFrame,
    TelemetryStreamParser,
    decode_frame,
)

from .mission_clock import MissionClock
from .models import PipelineDiagnostics, TelemetrySnapshot


_SEQUENCE_MODULUS = 1 << 32
_SEQUENCE_HALF_RANGE = 1 << 31


def _field_valid(
    frame: TelemetryFrame, flag: int, raw_value: int, sentinel: int
) -> bool:
    # Checking both sources protects consumers even from a malformed producer
    # that sets a validity bit while transmitting the reserved sentinel.
    return frame.valid(flag) and raw_value != sentinel


def _scaled(
    frame: TelemetryFrame,
    flag: int,
    raw_value: int,
    sentinel: int,
    divisor: float,
) -> float | None:
    if not _field_valid(frame, flag, raw_value, sentinel):
        return None
    return raw_value / divisor


def _integer(
    frame: TelemetryFrame,
    flag: int,
    raw_value: int,
    sentinel: int,
) -> int | None:
    if not _field_valid(frame, flag, raw_value, sentinel):
        return None
    return raw_value


def _error_bitmask(frame: TelemetryFrame) -> int | None:
    """Return the complete 8-bit fault mask when it is present.

    Unlike scalar u8 fields, 0xFF is meaningful here: it reports all eight
    defined faults.  Validity is therefore determined solely by its flag.
    """

    return frame.error_code if frame.valid(VALID_ERROR_CODE) else None


class TelemetryPipeline:
    """Decode arbitrary serial chunks into presentation-ready snapshots."""

    def __init__(self) -> None:
        self._parser = TelemetryStreamParser()
        self._mission_clock = MissionClock()
        self.reset()

    def reset(self) -> None:
        self._parser.reset()
        self._mission_clock.reset()
        self._total_frames = 0
        self._v2_frames = 0
        self._decode_failures = 0
        self._boot_sessions = 0
        self._boot_changes = 0
        self._sequence_gaps = 0
        self._duplicate_sequences = 0
        self._out_of_order_sequences = 0
        self._current_boot_id: int | None = None
        self._last_sequence: int | None = None

    @property
    def diagnostics(self) -> PipelineDiagnostics:
        return PipelineDiagnostics(
            total_frames=self._total_frames,
            v2_frames=self._v2_frames,
            decode_failures=self._decode_failures,
            boot_sessions=self._boot_sessions,
            boot_changes=self._boot_changes,
            sequence_gaps=self._sequence_gaps,
            duplicate_sequences=self._duplicate_sequences,
            out_of_order_sequences=self._out_of_order_sequences,
            parser_crc_failures=self._parser.total_crc_fail,
            parser_resync_count=self._parser.total_resync_count,
            current_boot_id=self._current_boot_id,
            last_sequence=self._last_sequence,
        )

    def feed(self, data: bytes) -> list[TelemetrySnapshot]:
        """Consume any byte chunk and return zero or more complete snapshots."""

        snapshots: list[TelemetrySnapshot] = []
        for wire_frame in self._parser.feed(data):
            try:
                frame = decode_frame(wire_frame)
            except FrameDecodeError:
                # Normally unreachable because the shared stream parser already
                # validates frames, but keep diagnostics honest at this boundary.
                self._decode_failures += 1
                continue

            self._total_frames += 1
            self._v2_frames += 1
            boot_session, should_emit = self._observe_v2_sequence(frame)
            if not should_emit:
                continue

            snapshots.append(self._snapshot(frame, boot_session))
        return snapshots

    def _observe_v2_sequence(
        self, frame: TelemetryFrame
    ) -> tuple[int, bool]:
        assert frame.boot_id is not None
        assert frame.sequence is not None

        if self._current_boot_id != frame.boot_id:
            if self._current_boot_id is not None:
                self._boot_changes += 1
            self._boot_sessions += 1
            self._current_boot_id = frame.boot_id
            self._last_sequence = frame.sequence
            self._mission_clock.reset()
            return self._boot_sessions, True

        assert self._last_sequence is not None
        delta = (frame.sequence - self._last_sequence) % _SEQUENCE_MODULUS
        if delta == 0:
            self._duplicate_sequences += 1
        elif delta < _SEQUENCE_HALF_RANGE:
            if delta > 1:
                self._sequence_gaps += delta - 1
            self._last_sequence = frame.sequence
        else:
            self._out_of_order_sequences += 1
        if delta == 0 or delta >= _SEQUENCE_HALF_RANGE:
            # Stale frames remain visible in diagnostics but must not mutate
            # MissionClock or any downstream application/UI state.
            return self._boot_sessions, False
        return self._boot_sessions, True

    def _snapshot(
        self, frame: TelemetryFrame, boot_session: int | None
    ) -> TelemetrySnapshot:
        latitude_deg = _scaled(
            frame, VALID_GPS_POSITION, frame.lat_raw, INVALID_I32, 1e7
        )
        longitude_deg = _scaled(
            frame, VALID_GPS_POSITION, frame.lon_raw, INVALID_I32, 1e7
        )
        gps_altitude_m = _scaled(
            frame, VALID_GPS_ALTITUDE, frame.gps_alt_dm, INVALID_I16, 10.0
        )
        barometric_altitude_m = _scaled(
            frame,
            VALID_BARO_ALTITUDE,
            frame.baro_alt_dm,
            INVALID_I16,
            10.0,
        )
        flight_state = _integer(
            frame, VALID_FLIGHT_STATE, frame.flight_state, INVALID_U8
        )
        water_raw = _integer(
            frame, VALID_WATER, frame.water_detected, INVALID_U8
        )
        mission_time_s = self._mission_clock.update(
            flight_state, frame.uptime_ms / 1000.0
        )

        return TelemetrySnapshot(
            protocol_version=frame.protocol_version,
            source_flags=frame.source_flags,
            boot_id=frame.boot_id,
            sequence=frame.sequence,
            uptime_ms=frame.uptime_ms,
            valid_flags=frame.valid_flags,
            boot_session=boot_session,
            mission_time_s=mission_time_s,
            latitude_deg=latitude_deg,
            longitude_deg=longitude_deg,
            gps_altitude_m=gps_altitude_m,
            gps_speed_mps=_scaled(
                frame,
                VALID_GPS_SPEED,
                frame.gps_speed_dms,
                INVALID_I16,
                10.0,
            ),
            satellite_count=_integer(
                frame,
                VALID_SATELLITE_COUNT,
                frame.sat_count,
                INVALID_U8,
            ),
            roll_deg=_scaled(
                frame, VALID_ATTITUDE, frame.roll_cdeg, INVALID_I16, 100.0
            ),
            pitch_deg=_scaled(
                frame, VALID_ATTITUDE, frame.pitch_cdeg, INVALID_I16, 100.0
            ),
            heading_deg=_scaled(
                frame, VALID_HEADING, frame.yaw_ddeg, INVALID_U16, 10.0
            ),
            gyro_x_dps=_scaled(
                frame, VALID_GYRO, frame.gyro_x_ddeg_s, INVALID_I16, 10.0
            ),
            gyro_y_dps=_scaled(
                frame, VALID_GYRO, frame.gyro_y_ddeg_s, INVALID_I16, 10.0
            ),
            gyro_z_dps=_scaled(
                frame, VALID_GYRO, frame.gyro_z_ddeg_s, INVALID_I16, 10.0
            ),
            acceleration_x_g=_scaled(
                frame, VALID_ACCELERATION, frame.accx_cg, INVALID_I16, 100.0
            ),
            acceleration_y_g=_scaled(
                frame, VALID_ACCELERATION, frame.accy_cg, INVALID_I16, 100.0
            ),
            acceleration_z_g=_scaled(
                frame, VALID_ACCELERATION, frame.accz_cg, INVALID_I16, 100.0
            ),
            barometric_altitude_m=barometric_altitude_m,
            battery_v=_scaled(
                frame, VALID_BATTERY, frame.battery_mv, INVALID_U16, 1000.0
            ),
            servo_power_v=_scaled(
                frame,
                VALID_SERVO_POWER,
                frame.servo_power_mv,
                INVALID_U16,
                1000.0,
            ),
            servo_angle_deg=_scaled(
                frame,
                VALID_SERVO_ANGLE,
                frame.servo_angle_ddeg,
                INVALID_I16,
                10.0,
            ),
            flight_state=flight_state,
            error_code=_error_bitmask(frame),
            water_detected=None if water_raw is None else bool(water_raw),
            primary_altitude_m=(
                barometric_altitude_m
                if barometric_altitude_m is not None
                else gps_altitude_m
            ),
        )
