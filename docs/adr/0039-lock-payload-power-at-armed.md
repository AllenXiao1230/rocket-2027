# Lock payload power at ARMED

The isolated low-power payload interface may be configured in `SAFE` and `READY`, then locks at `ARMED` until mission end. Neither Flight Computer nor Ground Station can switch payload power in flight, and payload power state never affects flight-critical logic.

## Consequences

Payload faults are isolated mission-data failures. Payload power policy does not provide an in-flight command or reset path around the Ground Command Window.
