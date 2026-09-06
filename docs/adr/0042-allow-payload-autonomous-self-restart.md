# Allow payload autonomous self-restart within its isolation boundary

Once `ARMED`, the Payload may autonomously restart using only its own isolated local behavior. It may not change the locked payload interface power configuration, request a Flight Computer or Ground Station action, or affect flight-critical logic; this preserves payload continuity without creating an in-flight avionics control path.

## Consequences

Payload restart and health remain Payload-Out-of-Band. A payload failure is still an isolated mission-data failure, while recovery and avionics telemetry continue independently.
