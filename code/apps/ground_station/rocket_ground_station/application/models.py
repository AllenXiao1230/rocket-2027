"""Typed values exchanged between the application and presentation layers."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class TelemetrySnapshot:
    """One decoded telemetry sample in engineering units.

    Sensor values are ``None`` whenever their v2 validity bit is clear or their
    wire value is an invalid sentinel.
    """

    protocol_version: int
    source_flags: int
    boot_id: int
    sequence: int
    uptime_ms: int
    valid_flags: int
    boot_session: int
    mission_time_s: float

    latitude_deg: float | None
    longitude_deg: float | None
    gps_altitude_m: float | None
    gps_speed_mps: float | None
    satellite_count: int | None

    roll_deg: float | None
    pitch_deg: float | None
    heading_deg: float | None
    gyro_x_dps: float | None
    gyro_y_dps: float | None
    gyro_z_dps: float | None
    acceleration_x_g: float | None
    acceleration_y_g: float | None
    acceleration_z_g: float | None

    barometric_altitude_m: float | None
    battery_v: float | None
    servo_power_v: float | None
    servo_angle_deg: float | None
    flight_state: int | None
    error_code: int | None
    water_detected: bool | None
    primary_altitude_m: float | None

    @property
    def uptime_s(self) -> float:
        return self.uptime_ms / 1000.0

    @property
    def primary_altitude(self) -> float | None:
        """Compatibility-friendly short name for the selected altitude."""

        return self.primary_altitude_m

    @property
    def gps_position(self) -> tuple[float, float] | None:
        if self.latitude_deg is None or self.longitude_deg is None:
            return None
        return self.latitude_deg, self.longitude_deg


@dataclass(frozen=True)
class PipelineDiagnostics:
    """Immutable view of parser, protocol, and sequence health counters."""

    total_frames: int = 0
    v2_frames: int = 0
    decode_failures: int = 0
    boot_sessions: int = 0
    boot_changes: int = 0
    sequence_gaps: int = 0
    duplicate_sequences: int = 0
    out_of_order_sequences: int = 0
    parser_crc_failures: int = 0
    parser_resync_count: int = 0
    current_boot_id: int | None = None
    last_sequence: int | None = None

    @property
    def sequence_anomalies(self) -> int:
        return (
            self.sequence_gaps
            + self.duplicate_sequences
            + self.out_of_order_sequences
        )
