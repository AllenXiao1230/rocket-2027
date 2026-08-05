"""Compatibility exports for pure flight-state application logic.

The former QObject/parser bridge duplicated telemetry decoding inside the UI
layer and had no production consumer.  Raw bytes now flow exclusively through
TelemetryPipeline.
"""

from rocket_ground_station.application.flight_state import (
    ErrorBit,
    FlightState,
    coerce_flight_state,
    decode_error,
    decode_state,
)

__all__ = [
    "ErrorBit",
    "FlightState",
    "coerce_flight_state",
    "decode_error",
    "decode_state",
]
