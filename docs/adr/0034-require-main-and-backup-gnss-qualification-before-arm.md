# Require Main and Backup GNSS qualification before ARM

Before `ARMED`, Main must have valid shared-GNSS data and Backup must independently complete its read-only GNSS qualification; either failure rejects the ARM request. This provides launch-time positioning and timing confidence while ADR-0029 still prevents later GNSS loss from blocking recovery.

## Consequences

The exact freshness, integrity and validity criteria are validation-gated TBDs. Backup never controls the shared GNSS MUX as part of its qualification.
