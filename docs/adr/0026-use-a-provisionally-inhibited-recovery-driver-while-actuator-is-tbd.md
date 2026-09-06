# Use a provisionally inhibited Recovery Driver while the actuator is TBD

While the release actuator remains subject to the 2027 Competition Acceptance Gate, the design may install a generic Recovery Driver and actuator terminal only behind an independent hardware inhibit, with no physical actuator connected. This creates an integration and test boundary without treating the path as flight-ready or authorizing deployment.

## Consequences

Actuator selection, driver topology, energy source and final release-path qualification remain TBD. Until selection, Non-Actuating Driver Bring-Up is limited to passive electrical integrity, hardware-inhibit readback and telemetry observability; driver fault injection is deferred until the actuator is selected and the test configuration represents the intended release path. Ground Test Session authorization cannot bypass the physical inhibit or turn an unapproved actuator selection into a flight configuration.
