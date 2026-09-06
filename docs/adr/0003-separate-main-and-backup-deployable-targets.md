# Separate Main and Backup into deployable targets

The repository will contain `firmware/main_flight_computer/` and `firmware/backup_flight_computer/`, with shared pure logic in `firmware/flight_core/`. Separate deployable targets make board configuration, calibration, power/RF dependencies and release evidence explicit while preserving a single tested implementation of common state, recovery and protocol logic.
