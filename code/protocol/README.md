# Telemetry Protocol

本目錄是 Rocket 2027 flight、Ground Receiver 與 Ground Station 共用的 wire-format source of truth。

## 版本與相容性

| Version | Size | Sync | Integrity | Status |
| --- | ---: | --- | --- | --- |
| v1 | 47 bytes | `55 AA` | XOR checksum | frozen、read compatibility only |
| v2 | 64 bytes | `A5 5A` | CRC-16/CCITT-FALSE | production transmit format |

Flight Computer 預設只送 v2；Ground Receiver 與 Ground Station 必須同時接受合法 v1/v2。v2 使用 `valid_flags`、deterministic sentinel、`boot_id` 與 `sequence` 表達 validity、session 與 link diagnostics。

## 目錄責任

```text
schema/telemetry_v2.json         canonical schema
tools/generate_protocol.py       generated-code / golden-vector generator
tools/check_schema.py            schema and consumer consistency check
python/rocket_protocol/          shared Python parser/encoder
cpp/                             shared C++ encoder/constants
golden_frames/vectors.json       cross-language golden vectors
tests/                           Python regression tests
telemetry_v1.md                  frozen v1 contract
telemetry_v2.md                  current v2 wire documentation
```

Generated offset、flag、CRC 或 sentinel constants 不得在 consumer 內手工複製。修改 schema 後必須重新產生並驗證所有 generated files。

## 驗證

從 repository root 執行：

```bash
python3 protocol/tools/generate_protocol.py --check
python3 protocol/tools/check_schema.py
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=protocol/python \
  python3 -m unittest discover -s protocol/tests -q
```

`--check` 失敗代表 schema、generated constants 或 golden vectors 發生 drift；在修正並通過前不得燒錄 production firmware。
