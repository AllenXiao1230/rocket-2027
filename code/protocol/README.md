# Telemetry Protocol

本目錄是 Rocket 2027 flight、Ground Receiver 與 Ground Station 共用的 wire-format source of truth。

## 正式協定

本專案僅使用 telemetry v2：64 bytes、sync `A5 5A`、version/length/message type header、little-endian 與 CRC-16/CCITT-FALSE。Flight Computer 對 LoRa 與 UART 傳送同一個 v2 frame；Ground Receiver 與 Ground Station 只接受通過 v2 驗證的 frame。

v2 使用 `valid_flags`、deterministic sentinel、`boot_id` 與 `sequence` 表達 validity、session 與 link diagnostics。

## 目錄責任

```text
schema/telemetry_v2.json         canonical schema
tools/generate_protocol.py       generated-code / golden-vector generator
tools/check_schema.py            schema and consumer consistency check
python/rocket_protocol/          shared Python parser/encoder
cpp/                             shared C++ encoder/constants
golden_frames/vectors.json       cross-language golden vectors
tests/                           Python regression tests
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
