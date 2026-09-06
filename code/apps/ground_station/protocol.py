"""Compatibility adapter for the shared telemetry protocol package.

The canonical implementation lives in ``protocol/python/rocket_protocol``.
Existing imports from ``ground_station.protocol`` remain supported and use the
canonical telemetry v2 stream parser.
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
    V2_FRAME_LENGTH,
    V2_SYNC,
)


# v2-only compatibility names for callers that still import this adapter.
FRAME_HEADER_0 = V2_SYNC[0]
FRAME_HEADER_1 = V2_SYNC[1]
FRAME_SYNC = V2_SYNC
PACKET_LEN = V2_FRAME_LENGTH
INVALID_BARO_ALT_DM = INVALID_I16

# Existing test/build API, now bound to the canonical v2 encoder.
build_frame = build_v2_frame
