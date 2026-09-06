# Retire the single-flight-computer production target

The existing single `firmware/flight_computer` ObserveOnly target is no longer the target architecture and will be moved to archive rather than transformed in place. This preserves it as historical evidence while preventing its single-controller assumptions from being mistaken for the dual-flight-computer design; no recovery actuation implementation is authorized by this decision.
