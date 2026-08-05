"""Raw serial byte transport with callback events.

The worker owns the serial object and background thread.  It emits received
bytes unchanged and intentionally knows nothing about telemetry framing,
application state, or UI frameworks.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import threading
from typing import Any, Callable, Protocol


class SerialEventKind(str, Enum):
    CONNECTED = "connected"
    BYTES_RECEIVED = "bytes_received"
    DISCONNECTED = "disconnected"
    ERROR = "error"


@dataclass(frozen=True)
class SerialEvent:
    kind: SerialEventKind
    data: bytes = b""
    message: str = ""


@dataclass(frozen=True)
class SerialWorkerConfig:
    port: str
    baudrate: int = 115200
    read_timeout_s: float = 0.1
    write_timeout_s: float = 0.2
    poll_interval_s: float = 0.01
    read_chunk_size: int = 4096

    def __post_init__(self) -> None:
        if not isinstance(self.port, str) or not self.port.strip():
            raise ValueError("port must be a non-empty string")
        if isinstance(self.baudrate, bool) or self.baudrate <= 0:
            raise ValueError("baudrate must be positive")
        if self.read_timeout_s < 0 or self.write_timeout_s < 0:
            raise ValueError("serial timeouts cannot be negative")
        if self.poll_interval_s < 0:
            raise ValueError("poll interval cannot be negative")
        if self.read_chunk_size <= 0:
            raise ValueError("read chunk size must be positive")


class _SerialLike(Protocol):
    in_waiting: int
    is_open: bool

    def read(self, size: int) -> bytes: ...

    def write(self, data: bytes) -> int | None: ...

    def close(self) -> None: ...


SerialFactory = Callable[..., _SerialLike]
SerialEventHandler = Callable[[SerialEvent], None]


def _default_serial_factory(**kwargs: Any) -> _SerialLike:
    try:
        import serial
    except ImportError as exc:  # pragma: no cover - depends on deployment env
        raise RuntimeError("PySerial is required for real serial I/O") from exc
    return serial.Serial(**kwargs)


class SerialWorker:
    """Own one serial connection and publish transport-level events."""

    def __init__(
        self,
        config: SerialWorkerConfig,
        event_handler: SerialEventHandler | None = None,
        *,
        serial_factory: SerialFactory | None = None,
    ) -> None:
        self.config = config
        self._event_handler = event_handler
        self._serial_factory = serial_factory or _default_serial_factory
        self._stop_requested = threading.Event()
        self._state_lock = threading.Lock()
        self._write_lock = threading.Lock()
        self._serial: _SerialLike | None = None
        self._thread: threading.Thread | None = None
        self._connected = False

    @property
    def is_running(self) -> bool:
        thread = self._thread
        return bool(thread and thread.is_alive())

    @property
    def is_connected(self) -> bool:
        with self._state_lock:
            return self._connected

    def set_event_handler(
        self, event_handler: SerialEventHandler | None
    ) -> None:
        """Replace the event sink, for example when attaching a UI adapter."""

        with self._state_lock:
            self._event_handler = event_handler

    def start(self) -> None:
        if self.is_running:
            raise RuntimeError("serial worker is already running")
        self._stop_requested.clear()
        self._thread = threading.Thread(
            target=self.run_forever,
            name=f"serial:{self.config.port}",
            daemon=True,
        )
        self._thread.start()

    def run_forever(self) -> None:
        """Open and service the connection in the current thread."""

        serial_port: _SerialLike | None = None
        connected = False
        try:
            serial_port = self._serial_factory(
                port=self.config.port,
                baudrate=self.config.baudrate,
                timeout=self.config.read_timeout_s,
                write_timeout=self.config.write_timeout_s,
                rtscts=False,
                dsrdtr=False,
                xonxoff=False,
            )
            self._configure_line_state(serial_port)
            with self._state_lock:
                self._serial = serial_port
                self._connected = True
            connected = True
            self._emit(
                SerialEvent(
                    SerialEventKind.CONNECTED,
                    message=f"Connected to {self.config.port}",
                )
            )

            while not self._stop_requested.is_set():
                try:
                    if not bool(getattr(serial_port, "is_open", True)):
                        raise OSError("serial port closed unexpectedly")
                    waiting = int(getattr(serial_port, "in_waiting", 0) or 0)
                    if waiting > 0:
                        size = min(waiting, self.config.read_chunk_size)
                        data = serial_port.read(size)
                        if data:
                            self._emit(
                                SerialEvent(
                                    SerialEventKind.BYTES_RECEIVED,
                                    data=bytes(data),
                                )
                            )
                            continue
                except Exception as exc:
                    self._emit(
                        SerialEvent(
                            SerialEventKind.ERROR,
                            message=f"Serial read failed: {exc}",
                        )
                    )
                    break

                self._stop_requested.wait(self.config.poll_interval_s)
        except Exception as exc:
            self._emit(
                SerialEvent(
                    SerialEventKind.ERROR,
                    message=f"Serial open failed: {exc}",
                )
            )
        finally:
            with self._state_lock:
                self._connected = False
                self._serial = None
            if serial_port is not None:
                try:
                    serial_port.close()
                except Exception:
                    pass
            if connected:
                self._emit(
                    SerialEvent(
                        SerialEventKind.DISCONNECTED,
                        message="Serial disconnected",
                    )
                )

    def write(self, data: bytes | bytearray | memoryview) -> bool:
        """Write raw bytes; return ``False`` when no connection is available."""

        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("serial transport accepts bytes-like data only")
        payload = bytes(data)
        if not payload:
            raise ValueError("serial write payload cannot be empty")

        with self._state_lock:
            serial_port = self._serial
            connected = self._connected
        if (
            serial_port is None
            or not connected
            or not bool(getattr(serial_port, "is_open", True))
        ):
            return False

        try:
            with self._write_lock:
                written = serial_port.write(payload)
            if written is not None and written != len(payload):
                raise OSError(
                    f"partial serial write: {written}/{len(payload)} bytes"
                )
            return True
        except Exception as exc:
            self._emit(
                SerialEvent(
                    SerialEventKind.ERROR,
                    message=f"Serial write failed: {exc}",
                )
            )
            self._stop_requested.set()
            return False

    def stop(self, timeout_s: float | None = None) -> bool:
        """Request shutdown and return whether the worker has stopped."""

        self._stop_requested.set()
        thread = self._thread
        if thread is not None and thread is not threading.current_thread():
            thread.join(timeout_s)
        return not self.is_running

    @staticmethod
    def _configure_line_state(serial_port: _SerialLike) -> None:
        for attribute in ("dtr", "rts"):
            try:
                setattr(serial_port, attribute, False)
            except Exception:
                pass

    def _emit(self, event: SerialEvent) -> None:
        with self._state_lock:
            handler = self._event_handler
        if handler is None:
            return
        try:
            handler(event)
        except Exception:
            # Consumer failures must not kill or change the transport loop.
            pass
