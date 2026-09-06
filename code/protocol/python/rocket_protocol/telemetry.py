from __future__ import annotations

from dataclasses import dataclass
import struct

from .generated import *


class FrameDecodeError(ValueError):
    pass


@dataclass(frozen=True)
class TelemetryFrame:
    protocol_version: int
    source_flags: int
    boot_id: int
    sequence: int
    uptime_ms: int
    valid_flags: int
    lat_raw: int
    lon_raw: int
    gps_alt_dm: int
    gps_speed_dms: int
    sat_count: int
    roll_cdeg: int
    pitch_cdeg: int
    yaw_ddeg: int
    gyro_x_ddeg_s: int
    gyro_y_ddeg_s: int
    gyro_z_ddeg_s: int
    accx_cg: int
    accy_cg: int
    accz_cg: int
    baro_alt_dm: int
    battery_mv: int
    servo_power_mv: int
    servo_angle_ddeg: int
    flight_state: int
    error_code: int
    water_detected: int

    def valid(self, flag: int) -> bool:
        return bool(self.valid_flags & flag)

    @property
    def time_s(self) -> float:
        return self.uptime_ms / 1000.0

    @property
    def lat_deg(self) -> float:
        return self.lat_raw / 1e7

    @property
    def lon_deg(self) -> float:
        return self.lon_raw / 1e7

    @property
    def gps_alt_m(self) -> float:
        return self.gps_alt_dm / 10.0

    @property
    def gps_speed_ms(self) -> float:
        return self.gps_speed_dms / 10.0

    @property
    def heading_deg(self) -> float:
        return (self.yaw_ddeg / 10.0) % 360.0

    @property
    def roll_deg(self) -> float:
        return self.roll_cdeg / 100.0

    @property
    def pitch_deg(self) -> float:
        return self.pitch_cdeg / 100.0

    @property
    def gyro_x_dps(self) -> float:
        return self.gyro_x_ddeg_s / 10.0

    @property
    def gyro_y_dps(self) -> float:
        return self.gyro_y_ddeg_s / 10.0

    @property
    def gyro_z_dps(self) -> float:
        return self.gyro_z_ddeg_s / 10.0

    @property
    def accx_g(self) -> float:
        return self.accx_cg / 100.0

    @property
    def accy_g(self) -> float:
        return self.accy_cg / 100.0

    @property
    def accz_g(self) -> float:
        return self.accz_cg / 100.0

    @property
    def has_baro_alt(self) -> bool:
        return self.valid(VALID_BARO_ALTITUDE)

    @property
    def baro_alt_m(self) -> float | None:
        return self.baro_alt_dm / 10.0 if self.has_baro_alt else None

    @property
    def battery_v(self) -> float:
        return self.battery_mv / 1000.0

    @property
    def servo_power_v(self) -> float:
        return self.servo_power_mv / 1000.0

    @property
    def servo_angle_deg(self) -> float:
        return self.servo_angle_ddeg / 10.0


def crc16_ccitt_false(data: bytes | bytearray) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _value(valid_flags: int, flag: int, value: int, invalid: int) -> int:
    return value if valid_flags & flag else invalid


def build_v2_frame(
    *,
    source_flags: int = 0,
    boot_id: int = 0,
    sequence: int = 0,
    uptime_ms: int = 0,
    valid_flags: int = 0,
    lat_raw: int = INVALID_I32,
    lon_raw: int = INVALID_I32,
    gps_alt_dm: int = INVALID_I16,
    gps_speed_dms: int = INVALID_I16,
    sat_count: int = INVALID_U8,
    roll_cdeg: int = INVALID_I16,
    pitch_cdeg: int = INVALID_I16,
    yaw_ddeg: int = INVALID_U16,
    gyro_x_ddeg_s: int = INVALID_I16,
    gyro_y_ddeg_s: int = INVALID_I16,
    gyro_z_ddeg_s: int = INVALID_I16,
    accx_cg: int = INVALID_I16,
    accy_cg: int = INVALID_I16,
    accz_cg: int = INVALID_I16,
    baro_alt_dm: int = INVALID_I16,
    battery_mv: int = INVALID_U16,
    servo_power_mv: int = INVALID_U16,
    servo_angle_ddeg: int = INVALID_I16,
    flight_state: int = INVALID_U8,
    error_code: int = INVALID_U8,
    water_detected: int = INVALID_U8,
) -> bytes:
    frame = bytearray(V2_FRAME_LENGTH)
    frame[:2] = V2_SYNC
    frame[2] = V2_VERSION
    frame[3] = V2_FRAME_LENGTH
    frame[4] = V2_MESSAGE_TYPE_TELEMETRY
    frame[5] = source_flags & 0xFF
    struct.pack_into("<HIII", frame, 6, boot_id & 0xFFFF, sequence & 0xFFFFFFFF,
                     uptime_ms & 0xFFFFFFFF, valid_flags & 0xFFFFFFFF)
    struct.pack_into("<ii", frame, 20,
                     _value(valid_flags, VALID_GPS_POSITION, lat_raw, INVALID_I32),
                     _value(valid_flags, VALID_GPS_POSITION, lon_raw, INVALID_I32))
    struct.pack_into("<hhB", frame, 28,
                     _value(valid_flags, VALID_GPS_ALTITUDE, gps_alt_dm, INVALID_I16),
                     _value(valid_flags, VALID_GPS_SPEED, gps_speed_dms, INVALID_I16),
                     _value(valid_flags, VALID_SATELLITE_COUNT, sat_count, INVALID_U8))
    struct.pack_into("<hhH", frame, 33,
                     _value(valid_flags, VALID_ATTITUDE, roll_cdeg, INVALID_I16),
                     _value(valid_flags, VALID_ATTITUDE, pitch_cdeg, INVALID_I16),
                     _value(valid_flags, VALID_HEADING, yaw_ddeg, INVALID_U16))
    struct.pack_into("<hhhhhh", frame, 39,
                     _value(valid_flags, VALID_GYRO, gyro_x_ddeg_s, INVALID_I16),
                     _value(valid_flags, VALID_GYRO, gyro_y_ddeg_s, INVALID_I16),
                     _value(valid_flags, VALID_GYRO, gyro_z_ddeg_s, INVALID_I16),
                     _value(valid_flags, VALID_ACCELERATION, accx_cg, INVALID_I16),
                     _value(valid_flags, VALID_ACCELERATION, accy_cg, INVALID_I16),
                     _value(valid_flags, VALID_ACCELERATION, accz_cg, INVALID_I16))
    struct.pack_into("<hHHh", frame, 51,
                     _value(valid_flags, VALID_BARO_ALTITUDE, baro_alt_dm, INVALID_I16),
                     _value(valid_flags, VALID_BATTERY, battery_mv, INVALID_U16),
                     _value(valid_flags, VALID_SERVO_POWER, servo_power_mv, INVALID_U16),
                     _value(valid_flags, VALID_SERVO_ANGLE, servo_angle_ddeg, INVALID_I16))
    frame[59] = _value(valid_flags, VALID_FLIGHT_STATE, flight_state, INVALID_U8)
    frame[60] = _value(valid_flags, VALID_ERROR_CODE, error_code, INVALID_U8)
    frame[61] = _value(valid_flags, VALID_WATER, water_detected, INVALID_U8)
    struct.pack_into("<H", frame, 62, crc16_ccitt_false(frame[:62]))
    return bytes(frame)


def is_valid_v2_frame(frame: bytes) -> bool:
    return (
        len(frame) == V2_FRAME_LENGTH
        and frame[:2] == V2_SYNC
        and frame[2] == V2_VERSION
        and frame[3] == V2_FRAME_LENGTH
        and frame[4] == V2_MESSAGE_TYPE_TELEMETRY
        and crc16_ccitt_false(frame[:62]) == struct.unpack_from("<H", frame, 62)[0]
    )


def is_valid_frame(frame: bytes) -> bool:
    return is_valid_v2_frame(frame)


def decode_v2_frame(frame: bytes) -> TelemetryFrame:
    if not is_valid_v2_frame(frame):
        raise FrameDecodeError("invalid telemetry v2 frame")
    return TelemetryFrame(
        protocol_version=2,
        source_flags=frame[5],
        boot_id=struct.unpack_from("<H", frame, 6)[0],
        sequence=struct.unpack_from("<I", frame, 8)[0],
        uptime_ms=struct.unpack_from("<I", frame, 12)[0],
        valid_flags=struct.unpack_from("<I", frame, 16)[0],
        lat_raw=struct.unpack_from("<i", frame, 20)[0],
        lon_raw=struct.unpack_from("<i", frame, 24)[0],
        gps_alt_dm=struct.unpack_from("<h", frame, 28)[0],
        gps_speed_dms=struct.unpack_from("<h", frame, 30)[0],
        sat_count=frame[32],
        roll_cdeg=struct.unpack_from("<h", frame, 33)[0],
        pitch_cdeg=struct.unpack_from("<h", frame, 35)[0],
        yaw_ddeg=struct.unpack_from("<H", frame, 37)[0],
        gyro_x_ddeg_s=struct.unpack_from("<h", frame, 39)[0],
        gyro_y_ddeg_s=struct.unpack_from("<h", frame, 41)[0],
        gyro_z_ddeg_s=struct.unpack_from("<h", frame, 43)[0],
        accx_cg=struct.unpack_from("<h", frame, 45)[0],
        accy_cg=struct.unpack_from("<h", frame, 47)[0],
        accz_cg=struct.unpack_from("<h", frame, 49)[0],
        baro_alt_dm=struct.unpack_from("<h", frame, 51)[0],
        battery_mv=struct.unpack_from("<H", frame, 53)[0],
        servo_power_mv=struct.unpack_from("<H", frame, 55)[0],
        servo_angle_ddeg=struct.unpack_from("<h", frame, 57)[0],
        flight_state=frame[59],
        error_code=frame[60],
        water_detected=frame[61],
    )


def decode_frame(frame: bytes) -> TelemetryFrame:
    return decode_v2_frame(frame)


class TelemetryStreamParser:
    def __init__(self):
        self._buffer = bytearray()
        self.total_crc_fail = 0
        self.total_resync_count = 0

    def reset(self) -> None:
        self._buffer.clear()
        self.total_crc_fail = 0
        self.total_resync_count = 0

    def feed(self, data: bytes) -> list[bytes]:
        frames: list[bytes] = []
        if data:
            self._buffer.extend(data)

        while True:
            if len(self._buffer) < 2:
                return frames

            start = self._buffer.find(V2_SYNC)
            if start < 0:
                keep = 1 if self._buffer[-1] == V2_SYNC[0] else 0
                if len(self._buffer) > keep:
                    self.total_resync_count += 1
                self._buffer[:] = self._buffer[-keep:] if keep else b""
                return frames

            if start:
                self.total_resync_count += 1
                del self._buffer[:start]

            if len(self._buffer) < V2_FRAME_LENGTH:
                return frames

            candidate = bytes(self._buffer[:V2_FRAME_LENGTH])
            if is_valid_v2_frame(candidate):
                frames.append(candidate)
                del self._buffer[:V2_FRAME_LENGTH]
            else:
                self.total_crc_fail += 1
                self.total_resync_count += 1
                del self._buffer[0]
