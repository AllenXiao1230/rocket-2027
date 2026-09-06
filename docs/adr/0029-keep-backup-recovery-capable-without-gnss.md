# Keep Backup Recovery-Capable without GNSS

The shared MAX-M10S GNSS and its power MUX are not prerequisites for Backup Recovery-Capable status or a lawful Backup recovery decision. Backup will use its independent IMU, barometer and flight-state evidence when GNSS is unavailable, while recording the GNSS/MUX fault for telemetry and post-flight analysis.

## Consequences

GNSS remains useful for positioning, timing and recovery search, but its failure cannot turn a recoverable vehicle into an authority deadlock. Any future second-GNSS proposal must be evaluated as a new architecture decision rather than an implicit dependency.
