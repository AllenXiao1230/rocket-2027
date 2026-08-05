"""Compatibility import for the layered application CSV logger."""

from rocket_ground_station.application.csv_logger import (
    TelemetryCSVLogger,
    TelemetryCsvLogger,
)


# Existing imports remain valid, but callers must now choose an output
# directory explicitly: CSVLogger(output_directory).
CSVLogger = TelemetryCsvLogger

__all__ = ["CSVLogger", "TelemetryCSVLogger", "TelemetryCsvLogger"]
