# Rocket 2027 Architecture

## Repository boundaries

```text
firmware/   deployable embedded targets
apps/       production operator applications
protocol/   shared wire-format source of truth
diagnostics hardware/host test tools, never linked into production
data/       controlled samples; runtime output goes to ignored data/runs
docs/       current cross-project documentation
archive/    preserved but non-production code
```

`archive/` 不可成為 production dependency。`diagnostics/` 可以依賴 production app 的 read-only presentation/serial components，但 production firmware/app 不得依賴 diagnostic 或 archive code。

## Production data flow

```text
flight sensors
  -> navigation estimators
  -> flight state / ObserveOnly criteria
  -> TelemetrySample
       -> shared v2 encoder -> LoRa -> ground_receiver -> serial bytes
       -> CSV formatter -> onboard SD

serial bytes
  -> Ground Station transport
  -> shared rocket_protocol parser
  -> typed application snapshot
  -> UI / host CSV
```

Telemetry v2 的唯一 wire-format 真源是 `protocol/schema/telemetry_v2.json`。Python/C++ constants、CRC、offset 與 validity flags 不可在 consumer 內另行複製。

## Production components

- `firmware/flight_computer`：ESP32-S3 sensors、navigation、state、ObserveOnly safety、telemetry 與 storage orchestration。
- `firmware/ground_receiver`：LoRa v1/v2 validation 與 binary-only serial passthrough；downlink-only。
- `firmware/camera`：OV5640 UART status/capture/burst endpoint。
- `apps/ground_station`：serial transport、canonical protocol application pipeline、PyQt UI 與 CSV。

## Dependency rules

- Flight pure core 不依賴 Arduino、LoRa、SD 或 GUI。
- Ground `application` 與 `transport` 不依賴 PyQt。
- UI 不解析 wire bytes；只接受 typed optional snapshot。
- Missing/invalid 值保持 `None`／empty，不能轉成假零。
- Duplicate/out-of-order v2 frame 只更新 diagnostics，不改動 UI、MissionClock 或 CSV。
- `archive/unsafe_actuation` 不能被建置腳本、workspace 或 production include path 引用。

Component-specific details 分別位於 `firmware/flight_computer/ARCHITECTURE.md` 與 `apps/ground_station/ARCHITECTURE.md`。
