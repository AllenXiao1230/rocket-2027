# Keep optional payload out of Avionics Telemetry v3

Optional payload data and status, including presence, power and health, are outside the Avionics Telemetry v3 contract. V3 remains a complete fixed snapshot of the two Flight Computers and recovery system, so no future payload can enlarge, omit fields from or create an exception to the Common RF Packet Budget.

## Consequences

Payload integration must use an isolated, non-flight-critical recording or communications design. Ground Station flight-critical telemetry remains independently interpretable even when no payload is installed or the payload fails.
