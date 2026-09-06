# Merge independent RF telemetry in the Ground Station

Ground Station will receive Main and Backup telemetry through independent receiver and serial transports, then merge their presentation at the application layer. A shared RF gateway is rejected because it would add a common failure point and hide the source-specific session, health and recovery-authority evidence required by the architecture baseline.
