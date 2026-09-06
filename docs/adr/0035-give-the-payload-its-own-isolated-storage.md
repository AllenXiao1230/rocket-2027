# Give the payload its own isolated storage

Payload-Out-of-Band data will be retained only on payload-owned storage, never on Main or Backup microSD. This preserves the independent avionics recording paths and ensures payload capacity, corruption or failure cannot affect flight telemetry or recovery decisions.

## Consequences

Payload data recovery is a non-flight-critical post-flight concern. Any future payload communications or download interface remains outside Avionics Telemetry v3 and must preserve the Payload Isolation Boundary.
