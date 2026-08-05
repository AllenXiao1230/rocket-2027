"""Raw transport services for the ground station.

This package deliberately has no telemetry parser or UI dependency.  A UI
adapter may subscribe to :class:`SerialWorker` events and pass received bytes
to the application layer.
"""

from .serial_ports import (
    SerialPort,
    discover_serial_ports,
    port_label,
    port_priority,
    select_serial_port,
)
from .serial_worker import (
    SerialEvent,
    SerialEventKind,
    SerialWorker,
    SerialWorkerConfig,
)

__all__ = [
    "SerialEvent",
    "SerialEventKind",
    "SerialPort",
    "SerialWorker",
    "SerialWorkerConfig",
    "discover_serial_ports",
    "port_label",
    "port_priority",
    "select_serial_port",
]
