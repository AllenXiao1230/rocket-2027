# Restrict SAFE GNSS service to the wired port

Shared GNSS power and MUX configuration changes require `SAFE`, an authenticated service action, and the physical wired service port. This deliberately excludes RF and the Ground-Only Maintenance Link from changing the sole shared avionics component.

## Consequences

`READY` remains diagnostic-only and `ARMED` remains configuration-locked. A field operator needing to change GNSS configuration must attach the wired port and return the system to `SAFE` before repeating GNSS Pre-Arm Qualification.
