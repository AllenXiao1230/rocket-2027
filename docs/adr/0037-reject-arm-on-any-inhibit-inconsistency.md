# Reject ARM on any inhibit inconsistency

ARM is rejected and the system remains in `READY` whenever either Local Inhibit Readback is missing, the two readings disagree, or either reading conflicts with the physical Safe/Arm condition. Main, Backup and Ground Station cannot override this Pre-Arm Inhibit Consistency boundary.

## Consequences

The rejection reason must remain observable in both avionics snapshots and ground records. Resolving the physical cause is a prerequisite for another ARM attempt.
