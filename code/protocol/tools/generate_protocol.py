#!/usr/bin/env python3
"""Generate language constants from the canonical telemetry v2 schema."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "schema" / "telemetry_v2.json"
PYTHON_PATH = ROOT / "python" / "rocket_protocol" / "generated.py"
CPP_PATH = (
    ROOT / "cpp" / "TelemetryProtocol" / "src" / "TelemetryV2Generated.h"
)


def snake_constant(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", name).upper()


def pascal(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


def render_python(schema: dict) -> str:
    sync = bytes.fromhex(schema["sync_hex"])
    lines = [
        '"""Generated constants for telemetry v2.',
        "",
        "Source: protocol/schema/telemetry_v2.json",
        "Regenerate/check with protocol/tools/generate_protocol.py.",
        '"""',
        "",
        f'V2_SYNC = b"\\x{sync[0]:02x}\\x{sync[1]:02x}"',
        f'V2_VERSION = {schema["version"]}',
        f'V2_FRAME_LENGTH = {schema["frame_length"]}',
        f'V2_MESSAGE_TYPE_TELEMETRY = {schema["message_type"]}',
        "",
    ]
    for name, bit in schema["valid_flags"].items():
        lines.append(f"VALID_{snake_constant(name)} = 1 << {bit}")
    lines.append("")
    source_names = {
        "heading_from_imu": "SOURCE_HEADING_IMU",
        "heading_from_gps": "SOURCE_HEADING_GPS",
        "state_altitude_from_baro": "SOURCE_STATE_ALTITUDE_BARO",
        "state_altitude_from_gps": "SOURCE_STATE_ALTITUDE_GPS",
    }
    for name, bit in schema["source_flags"].items():
        lines.append(f"{source_names[name]} = 1 << {bit}")
    lines.extend(
        [
            "",
            "INVALID_I32 = -(1 << 31)",
            "INVALID_I16 = -(1 << 15)",
            "INVALID_U16 = (1 << 16) - 1",
            "INVALID_U8 = (1 << 8) - 1",
            "",
            "# Wire offsets are generated so parsers never duplicate the schema.",
        ]
    )
    for field in schema["fields"]:
        lines.append(
            f'OFFSET_{snake_constant(field["name"])} = {field["offset"]}'
        )
    checksum_end = schema["checksum"]["covered_offsets"][1]
    lines.append(f"CRC_COVERED_LENGTH = {checksum_end + 1}")
    lines.append("")
    return "\n".join(lines)


def render_cpp(schema: dict) -> str:
    sync = bytes.fromhex(schema["sync_hex"])
    lines = [
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "namespace rocket {",
        "namespace protocol {",
        "",
        f"constexpr uint8_t kTelemetryV2Sync0 = 0x{sync[0]:02X};",
        f"constexpr uint8_t kTelemetryV2Sync1 = 0x{sync[1]:02X};",
        f'constexpr uint8_t kTelemetryV2Version = {schema["version"]};',
        (
            "constexpr uint8_t kTelemetryV2FrameLength = "
            f'{schema["frame_length"]};'
        ),
        (
            "constexpr uint8_t kTelemetryV2MessageType = "
            f'{schema["message_type"]};'
        ),
        "enum TelemetryValidFlag : uint32_t {",
    ]
    for name, bit in schema["valid_flags"].items():
        lines.append(f"  kValid{pascal(name)} = 1UL << {bit},")
    lines.extend(["};", "", "enum TelemetrySourceFlag : uint8_t {"])
    source_names = {
        "heading_from_imu": "kSourceHeadingImu",
        "heading_from_gps": "kSourceHeadingGps",
        "state_altitude_from_baro": "kSourceStateAltitudeBaro",
        "state_altitude_from_gps": "kSourceStateAltitudeGps",
    }
    for name, bit in schema["source_flags"].items():
        lines.append(f"  {source_names[name]} = 1U << {bit},")
    lines.extend(
        [
            "};",
            "",
            "enum TelemetryV2Offset : uint8_t {",
        ]
    )
    for field in schema["fields"]:
        lines.append(
            f'  kOffset{pascal(field["name"])} = {field["offset"]},'
        )
    checksum_end = schema["checksum"]["covered_offsets"][1]
    lines.extend(
        [
            "};",
            "",
            f"constexpr uint8_t kCrcCoveredLength = {checksum_end + 1};",
            "constexpr int32_t kInvalidI32 = INT32_MIN;",
            "constexpr int16_t kInvalidI16 = INT16_MIN;",
            "constexpr uint16_t kInvalidU16 = UINT16_MAX;",
            "constexpr uint8_t kInvalidU8 = UINT8_MAX;",
            "",
            "}  // namespace protocol",
            "}  // namespace rocket",
            "",
        ]
    )
    return "\n".join(lines)


def check_or_write(path: Path, content: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == content:
        return True
    if check:
        print(f"generated file is stale: {path.relative_to(ROOT.parent)}")
        return False
    path.write_text(content, encoding="utf-8")
    print(f"generated {path.relative_to(ROOT.parent)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail without writing when generated files differ",
    )
    args = parser.parse_args()
    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    ok = check_or_write(PYTHON_PATH, render_python(schema), args.check)
    ok = check_or_write(CPP_PATH, render_cpp(schema), args.check) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
