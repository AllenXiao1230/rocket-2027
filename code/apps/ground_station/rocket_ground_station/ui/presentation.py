"""Pure presentation helpers shared by Qt widgets and headless tests."""

from __future__ import annotations

from rocket_ground_station.application.flight_state import (
    FlightState,
    coerce_flight_state,
)


UNKNOWN_STATE_TEXT = "UNKNOWN"
UNAVAILABLE_TEXT = "---"


def normalized_flight_state(value: object) -> FlightState | None:
    """Return a known flight state; unknown or invalid values become ``None``."""

    return coerce_flight_state(value)


def flight_state_name(value: object) -> str:
    """Return the operator-facing enum name without inventing an initial state."""

    state = normalized_flight_state(value)
    return UNKNOWN_STATE_TEXT if state is None else state.name


def format_optional(value: object, format_string: str) -> str:
    """Format a measurement or render the common unavailable marker."""

    if value is None:
        return UNAVAILABLE_TEXT
    try:
        return format_string.format(value)
    except (TypeError, ValueError):
        return UNAVAILABLE_TEXT


def speed_altitude_texts(
    gps_speed: object,
    baro_speed: object,
    gps_altitude: object,
    baro_altitude: object,
) -> tuple[str, str, str, str]:
    """Build the four speed/altitude labels without fabricating zeroes."""

    return (
        format_optional(gps_speed, "{:.2f}"),
        format_optional(baro_speed, "{:.2f}"),
        format_optional(gps_altitude, "{:.1f}"),
        format_optional(baro_altitude, "{:.1f}"),
    )
