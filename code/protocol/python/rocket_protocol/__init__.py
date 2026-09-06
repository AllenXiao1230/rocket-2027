from . import generated as _generated
from .generated import *
from .telemetry import (
    FrameDecodeError,
    TelemetryFrame,
    TelemetryStreamParser,
    build_v2_frame,
    crc16_ccitt_false,
    decode_frame,
    decode_v2_frame,
    is_valid_frame,
    is_valid_v2_frame,
)

__all__ = [
    "FrameDecodeError",
    "TelemetryFrame",
    "TelemetryStreamParser",
    "build_v2_frame",
    "crc16_ccitt_false",
    "decode_frame",
    "decode_v2_frame",
    "is_valid_frame",
    "is_valid_v2_frame",
] + [
    name for name in dir(_generated) if name.isupper()
]
