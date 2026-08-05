"""Pure application services for the rocket ground station.

This package deliberately has no GUI or serial-port dependencies.  Transport
code supplies byte chunks to :class:`TelemetryPipeline`; UI code consumes the
typed snapshots returned by it.
"""

from .flight_state import (
    ErrorBit,
    FlightState,
    coerce_flight_state,
    decode_error,
    decode_state,
)
from .mission_clock import MissionClock
from .models import PipelineDiagnostics, TelemetrySnapshot
from .telemetry_pipeline import TelemetryPipeline

__all__ = [
    "ErrorBit",
    "FlightState",
    "MissionClock",
    "PipelineDiagnostics",
    "TelemetryPipeline",
    "TelemetrySnapshot",
    "coerce_flight_state",
    "decode_error",
    "decode_state",
]
