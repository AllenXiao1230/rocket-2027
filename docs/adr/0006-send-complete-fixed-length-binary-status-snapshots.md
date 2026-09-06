# Send complete fixed-length binary status snapshots

Every v3 telemetry packet will be a fixed-length binary snapshot containing all current avionics system and recovery state needed for independent interpretation, including the latest outcome and sequence for each recovery/fault class. Optional payload data and status are Payload-Out-of-Band and do not appear in v3; the packet will not contain an unbounded mission event history, while Ground Station and both independent microSD logs retain the ordered snapshots needed to reconstruct avionics history.
