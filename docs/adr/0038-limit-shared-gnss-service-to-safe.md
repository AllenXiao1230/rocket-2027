# Limit shared GNSS service to SAFE

Shared GNSS power or MUX configuration changes require an explicit authenticated service action in `SAFE`; `READY` permits diagnosis and rereading only. Backup remains read-only and `ARMED` retains the configuration lock.

## Consequences

An invalid GNSS qualification in `READY` blocks ARM rather than triggering automatic recovery action. The operator must return to `SAFE` to change the shared GNSS configuration and then repeat qualification.
