"""Compatibility adapter for the shared telemetry protocol package.

The canonical implementation lives in ``protocol/python/rocket_protocol``.
Existing imports from ``ground_station.protocol`` remain supported while v1
and v2 are accepted by the same stream parser.
"""

from pathlib import Path
import sys


def _find_shared_protocol() -> Path:
    for parent in Path(__file__).resolve().parents:
        candidate = parent / "protocol" / "python"
        if (candidate / "rocket_protocol").is_dir():
            return candidate
    raise RuntimeError("cannot locate repository protocol/python directory")


_SHARED_PROTOCOL = _find_shared_protocol()
if str(_SHARED_PROTOCOL) not in sys.path:
    sys.path.insert(0, str(_SHARED_PROTOCOL))

from rocket_protocol import *  # noqa: E402,F401,F403
from rocket_protocol.generated import (  # noqa: E402
    INVALID_I16,
    V1_FRAME_LENGTH,
    V1_SYNC,
    V2_FRAME_LENGTH,
    V2_SYNC,
)
from rocket_protocol.telemetry import xor_checksum  # noqa: E402


# Frozen v1 compatibility names.
FRAME_HEADER_0 = V1_SYNC[0]
FRAME_HEADER_1 = V1_SYNC[1]
FRAME_SYNC = V1_SYNC
V2_FRAME_SYNC = V2_SYNC
PACKET_LEN = V1_FRAME_LENGTH
V2_PACKET_LEN = V2_FRAME_LENGTH
INVALID_BARO_ALT_DM = INVALID_I16

# Existing test/build API.
build_frame = build_v1_frame
_xor_crc = xor_checksum
