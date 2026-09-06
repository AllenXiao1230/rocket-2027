# Require fresh authorization after an inhibit fault

After any Pre-Arm Inhibit Consistency failure, resolving the physical mismatch is necessary but not sufficient to ARM. The next attempt must use a new Authorized Ground Command Session and repeat all Main and Backup pre-arm checks, so a stale grant cannot survive a safety-boundary fault.

## Consequences

The failed attempt, remediation, and later authorization remain distinguishable in avionics snapshots and ground records. There is no automatic re-arm or Ground Station clearance that reuses the prior grant.
