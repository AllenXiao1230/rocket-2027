#!/usr/bin/env python3
"""Fail CI when schema, generated constants, or golden vectors drift."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "schema" / "telemetry_v2.json"
GENERATED_PY = ROOT / "python" / "rocket_protocol" / "generated.py"
GENERATED_CPP = (
    ROOT
    / "cpp"
    / "TelemetryProtocol"
    / "src"
    / "TelemetryV2Generated.h"
)
GOLDEN_VECTORS = ROOT / "golden_frames" / "vectors.json"
CPP_GOLDEN_TEST = (
    ROOT.parent
    / "firmware"
    / "flight_computer"
    / "test"
    / "test_telemetry_protocol"
    / "test_main.cpp"
)


def load_generated_python():
    spec = importlib.util.spec_from_file_location(
        "rocket_protocol_generated", GENERATED_PY
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def main() -> int:
    subprocess.run(
        [sys.executable, str(ROOT / "tools" / "generate_protocol.py"), "--check"],
        check=True,
    )

    schema = json.loads(SCHEMA.read_text())
    assert schema["version"] == 2
    assert schema["frame_length"] == 64
    assert schema["sync_hex"].lower() == "a55a"

    occupied = [False] * schema["frame_length"]
    for field in schema["fields"]:
        start = field["offset"]
        end = start + field["size"]
        assert 0 <= start < end <= schema["frame_length"], field["name"]
        for offset in range(start, end):
            assert not occupied[offset], (field["name"], offset)
            occupied[offset] = True
    assert all(occupied), "schema must cover every byte exactly once"

    generated = load_generated_python()
    assert generated.V2_VERSION == schema["version"]
    assert generated.V2_FRAME_LENGTH == schema["frame_length"]
    assert generated.V2_SYNC.hex().upper() == schema["sync_hex"]

    python_flag_names = {
        "gps_position": "VALID_GPS_POSITION",
        "gps_altitude": "VALID_GPS_ALTITUDE",
        "gps_speed": "VALID_GPS_SPEED",
        "satellite_count": "VALID_SATELLITE_COUNT",
        "attitude": "VALID_ATTITUDE",
        "heading": "VALID_HEADING",
        "gyro": "VALID_GYRO",
        "acceleration": "VALID_ACCELERATION",
        "baro_altitude": "VALID_BARO_ALTITUDE",
        "battery": "VALID_BATTERY",
        "servo_power": "VALID_SERVO_POWER",
        "servo_angle": "VALID_SERVO_ANGLE",
        "flight_state": "VALID_FLIGHT_STATE",
        "error_code": "VALID_ERROR_CODE",
        "water": "VALID_WATER",
    }
    for schema_name, bit in schema["valid_flags"].items():
        assert getattr(generated, python_flag_names[schema_name]) == 1 << bit

    for field in schema["fields"]:
        offset_name = f"OFFSET_{field['name'].upper()}"
        assert getattr(generated, offset_name) == field["offset"]

    vectors = json.loads(GOLDEN_VECTORS.read_text())
    expected_hex = vectors["telemetry_v2_nominal"]["expected_hex"].lower()
    cpp_test = CPP_GOLDEN_TEST.read_text()
    array_match = re.search(
        r"kExpectedNominalFrame\[64\]\s*=\s*\{(.*?)\};",
        cpp_test,
        re.DOTALL,
    )
    assert array_match is not None
    cpp_golden_hex = "".join(
        re.findall(r"0x([0-9a-fA-F]{2})", array_match.group(1))
    )
    assert cpp_golden_hex.lower() == expected_hex

    cpp = GENERATED_CPP.read_text()
    cpp_constants = {
        name: int(value, 0)
        for name, value in re.findall(
            r"constexpr uint8_t (kTelemetryV2\w+) = ([^;]+);", cpp
        )
    }
    assert cpp_constants["kTelemetryV2Version"] == schema["version"]
    assert cpp_constants["kTelemetryV2FrameLength"] == schema["frame_length"]
    assert cpp_constants["kTelemetryV2MessageType"] == schema["message_type"]

    print("telemetry schema/generated constants/golden vectors: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
