# Lock the shared GNSS configuration at ARMED

The shared GNSS power and MUX configuration may be established and checked in `SAFE` and `READY`, then locks when the mission enters `ARMED`; neither Flight Computer performs an in-flight GNSS reset or MUX change. Because Backup is GNSS-Optional Recovery-Capable, this avoids creating a shared in-flight switching fault without blocking lawful recovery.

## Consequences

GNSS or MUX degradation after `ARMED` is telemetry and recovery-search evidence, not an automatic control action. Pre-arm checks must reject an invalid intended GNSS configuration while leaving recovery independence intact.
