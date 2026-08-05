"""Mission elapsed-time calculation isolated from widgets and timers."""

from __future__ import annotations

import math

from .flight_state import FlightState, coerce_flight_state


_FLIGHT_PHASES = frozenset(
    {
        FlightState.ASCENT,
        FlightState.APOGEE,
        FlightState.DESCENT,
        FlightState.LANDED,
    }
)


class MissionClock:
    """Derive T+ time from telemetry uptime and the first observed flight phase.

    A station connected after liftoff starts at T+0 on its first flight-phase
    sample because the protocol does not carry a separate liftoff timestamp.
    Returning to IDLE/PREFLIGHT starts a new mission.  A firmware uptime reset
    also starts a new local clock rather than producing a negative value.
    """

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self._launch_uptime_s: float | None = None
        self._last_uptime_s: float | None = None
        self._last_state: FlightState | None = None

    @property
    def started(self) -> bool:
        return self._launch_uptime_s is not None

    def update(
        self, state: int | FlightState | None, uptime_s: float
    ) -> float:
        """Observe one sample and return non-negative mission time in seconds."""

        if not math.isfinite(uptime_s) or uptime_s < 0:
            raise ValueError("uptime_s must be finite and non-negative")

        current = coerce_flight_state(state)
        uptime_reset = (
            self._last_uptime_s is not None
            and uptime_s + 0.001 < self._last_uptime_s
        )
        if uptime_reset:
            self._launch_uptime_s = None

        if current in (FlightState.IDLE, FlightState.PREFLIGHT):
            self._launch_uptime_s = None
        elif current in _FLIGHT_PHASES and self._launch_uptime_s is None:
            self._launch_uptime_s = uptime_s

        self._last_uptime_s = uptime_s
        self._last_state = current

        if self._launch_uptime_s is None:
            return 0.0
        return max(0.0, uptime_s - self._launch_uptime_s)
