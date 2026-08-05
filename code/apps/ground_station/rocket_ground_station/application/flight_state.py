"""Flight-state and fault decoding with no UI dependency."""

from __future__ import annotations

from enum import IntEnum, IntFlag


class FlightState(IntEnum):
    TEST = 0
    IDLE = 1
    PREFLIGHT = 2
    ASCENT = 3
    APOGEE = 4
    DESCENT = 5
    LANDED = 6
    ABORT = 99


class ErrorBit(IntFlag):
    NONE = 0
    LORA_INIT = 1 << 0
    SD_INIT = 1 << 1
    RTC_INIT = 1 << 2
    BARO_INIT = 1 << 3
    IMU_INIT = 1 << 4
    ADXL_INIT = 1 << 5
    GPS_NO_FIX = 1 << 6
    BATTERY = 1 << 7


_STATE_LABELS = {
    FlightState.TEST: "測試",
    FlightState.IDLE: "待命",
    FlightState.PREFLIGHT: "發射前",
    FlightState.ASCENT: "上升",
    FlightState.APOGEE: "過頂點",
    FlightState.DESCENT: "下降",
    FlightState.LANDED: "著陸",
    FlightState.ABORT: "中止",
}

_ERROR_LABELS = (
    (ErrorBit.LORA_INIT, "LORA"),
    (ErrorBit.SD_INIT, "SD"),
    (ErrorBit.RTC_INIT, "RTC"),
    (ErrorBit.BARO_INIT, "BARO"),
    (ErrorBit.IMU_INIT, "IMU"),
    (ErrorBit.ADXL_INIT, "ADXL"),
    (ErrorBit.GPS_NO_FIX, "GPS"),
    (ErrorBit.BATTERY, "BAT"),
)


def coerce_flight_state(value: int | None) -> FlightState | None:
    if value is None:
        return None
    try:
        return FlightState(value)
    except (TypeError, ValueError):
        return None


def decode_state(value: int | FlightState | None) -> str:
    """Return the established operator-facing label or ``UNKNOWN``."""

    state = coerce_flight_state(value)
    if state is None:
        return "UNKNOWN"
    return _STATE_LABELS[state]


def decode_error(value: int | None) -> str:
    """Decode the firmware's bitmask without discarding unknown bits."""

    if value is None:
        return "UNKNOWN"
    if value == 0:
        return "OK"
    known_mask = 0
    labels: list[str] = []
    for bit, label in _ERROR_LABELS:
        known_mask |= int(bit)
        if value & int(bit):
            labels.append(label)
    unknown = value & ~known_mask
    if unknown:
        labels.append(f"0x{unknown:02X}")
    return " | ".join(labels)
