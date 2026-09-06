# Partition the shared avionics enclosure

The shared avionics enclosure will use an internal divider to create separate Main and Backup zones, with independently fixed boards, power boards, harnesses and antenna feedlines. This limits an internal electrical, loose-hardware or service-side fault from directly crossing between the two controller paths, while retaining the common enclosure risks recorded in ADR-0025.

## Consequences

The divider, mounting and routing are part of the final flight configuration and must be included in environmental qualification. The divider does not turn the common enclosure into two fully independent enclosures.
