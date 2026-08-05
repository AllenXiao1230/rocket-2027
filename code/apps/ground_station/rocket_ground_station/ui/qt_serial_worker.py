"""Compatibility Qt adapter for the raw serial transport."""

from PyQt5.QtCore import QObject, pyqtSignal

from rocket_ground_station.transport import (
    SerialEvent,
    SerialEventKind,
    SerialWorker as TransportSerialWorker,
    SerialWorkerConfig,
)


class SerialWorker(QObject):
    """Preserve the legacy Qt API while delegating all I/O to transport."""

    data_received = pyqtSignal(bytes)
    status_changed = pyqtSignal(str)
    error_occurred = pyqtSignal(str)

    def __init__(self, port: str, baudrate: int = 115200, parent=None):
        super().__init__(parent)
        self.port = port
        self.baudrate = baudrate
        self._transport = TransportSerialWorker(
            SerialWorkerConfig(port=port, baudrate=baudrate),
            self._on_transport_event,
        )

    def start(self):
        self._transport.start()

    def write(self, data: bytes):
        return self._transport.write(data)

    def stop(self):
        self._transport.stop()

    def _on_transport_event(self, event: SerialEvent):
        if event.kind == SerialEventKind.BYTES_RECEIVED:
            self.data_received.emit(event.data)
        elif event.kind in (
            SerialEventKind.CONNECTED,
            SerialEventKind.DISCONNECTED,
        ):
            self.status_changed.emit(event.message)
        elif event.kind == SerialEventKind.ERROR:
            self.error_occurred.emit(event.message)
