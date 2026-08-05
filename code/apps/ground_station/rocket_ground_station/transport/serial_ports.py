"""Testable serial-port discovery and selection.

PySerial is imported only when real discovery is requested.  Tests and other
callers can inject an enumerator and therefore do not need PySerial installed.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Iterable


@dataclass(frozen=True)
class SerialPort:
    """UI-independent description of one serial device."""

    device: str
    description: str = ""
    hwid: str = ""
    vid: int | None = None
    pid: int | None = None
    serial_number: str | None = None
    manufacturer: str | None = None
    product: str | None = None
    location: str | None = None


PortEnumerator = Callable[[], Iterable[Any]]

_PREFERRED_USB_HINTS = (
    "wchusbserial",
    "usb serial",
    "usbserial",
    "usbmodem",
    "slab_usbtouart",
    "ttyusb",
    "ttyacm",
    "cp210",
    "ch340",
    "ftdi",
)
_LOW_PRIORITY_HINTS = ("bluetooth", "debug-console")


def _optional_text(value: Any) -> str | None:
    if value is None:
        return None
    text = str(value).strip()
    return text or None


def _optional_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _from_port_info(info: Any) -> SerialPort | None:
    if isinstance(info, SerialPort):
        return info if info.device.strip() else None

    device = str(getattr(info, "device", "") or "").strip()
    if not device:
        return None

    return SerialPort(
        device=device,
        description=str(getattr(info, "description", "") or "").strip(),
        hwid=str(getattr(info, "hwid", "") or "").strip(),
        vid=_optional_int(getattr(info, "vid", None)),
        pid=_optional_int(getattr(info, "pid", None)),
        serial_number=_optional_text(getattr(info, "serial_number", None)),
        manufacturer=_optional_text(getattr(info, "manufacturer", None)),
        product=_optional_text(getattr(info, "product", None)),
        location=_optional_text(getattr(info, "location", None)),
    )


def port_priority(port: SerialPort) -> tuple[int, str]:
    """Return a stable sort key that places likely USB UARTs first."""

    text = " ".join((port.device, port.description, port.hwid)).casefold()
    device = port.device.casefold()

    if any(token in text for token in _PREFERRED_USB_HINTS):
        return (0, device)
    if "usb" in text:
        return (1, device)
    if any(token in text for token in _LOW_PRIORITY_HINTS):
        return (3, device)
    return (2, device)


def port_label(port: SerialPort) -> str:
    """Build a display label without exposing PySerial objects to the UI."""

    description = port.description.strip()
    if not description or description.casefold() == "n/a":
        return port.device
    return f"{port.device} ({description})"


def _default_enumerator() -> Iterable[Any]:
    try:
        from serial.tools import list_ports
    except ImportError as exc:  # pragma: no cover - depends on deployment env
        raise RuntimeError(
            "PySerial is required for real serial-port discovery"
        ) from exc
    return list_ports.comports()


def discover_serial_ports(
    enumerator: PortEnumerator | None = None,
) -> tuple[SerialPort, ...]:
    """Discover, normalize, de-duplicate and prioritize serial ports."""

    provider = enumerator or _default_enumerator
    by_device: dict[str, SerialPort] = {}
    for info in provider():
        port = _from_port_info(info)
        if port is not None and port.device not in by_device:
            by_device[port.device] = port
    return tuple(sorted(by_device.values(), key=port_priority))


def select_serial_port(
    ports: Iterable[SerialPort],
    preferred_device: str | None = None,
) -> SerialPort | None:
    """Select an exact remembered device, otherwise the highest-priority port."""

    ordered = sorted(ports, key=port_priority)
    if preferred_device:
        for port in ordered:
            if port.device == preferred_device:
                return port
    return ordered[0] if ordered else None
