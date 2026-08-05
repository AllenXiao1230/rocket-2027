# Ground Station Architecture

## Responsibility

Ground Station 是 production downlink consumer 與 operator UI。它不決定 flight actuation，也不擁有 telemetry wire format；wire contract 由 repository root 的 `protocol/` 定義。

## Dependency direction

```text
rocket_ground_station.transport
  -> raw bytes and lifecycle events

protocol/python/rocket_protocol
  -> canonical v1/v2 validation and decode

rocket_ground_station.application
  -> typed snapshots, sequence policy, mission clock,
     command authorization and CSV records

rocket_ground_station.ui
  -> PyQt presentation and operator interactions
```

允許的組合方向是 `transport -> protocol -> application -> UI`。更精確地說，UI 可以組合前三層；application 只依賴 canonical protocol 與純 Python model；transport 不知道 protocol；兩者都不可 import PyQt。

## Modules

### `transport/`

- `serial_ports.py`：列舉、正規化、去重與排序 serial ports。
- `serial_worker.py`：UI-independent thread/callback transport。只收送 bytes，負責 open/read/write/close 與 lifecycle event。

Write exception 或 partial write 是 terminal connection failure：`ERROR -> stop -> close -> DISCONNECTED`。

### `application/`

- `models.py`：immutable `TelemetrySnapshot` 與 `PipelineDiagnostics`。
- `telemetry_pipeline.py`：mixed v1/v2 stream、validity conversion、boot/sequence tracking 與 stale-frame suppression。
- `mission_clock.py`：以有效 flight phase 建立 T+，並在 boot/session reset 時重新開始。
- `flight_state.py`：flight state 與 error bitmask 的純函式解碼。
- `commands.py`：connection-aware uplink authorization。LoRa receiver/unknown 永遠 downlink-only。
- `csv_logger.py`：固定 schema、explicit output directory、collision-safe filename 與 fail-visible I/O。

Pipeline 對 duplicate/out-of-order v2 frame 只增加 diagnostics，不建立 snapshot，因此它們不能回捲 MissionClock、覆寫 UI 或寫入 CSV。

### `ui/`

- `qt_serial_worker.py`：把 pure transport event 轉成 Qt signals。
- `main_window.py`：組合 panel、pipeline、transport 與 logger。
- `presentation.py`：不依賴 Qt 的顯示規則，讓 `UNKNOWN`、`---` 與真實零值可以 headless 測試。

Top-level Qt widgets 暫時保留供 MainWindow 與 diagnostic tools 使用。`SerialPanel` 的 connection-mode selector 預設隱藏；只有 production MainWindow 明確開啟，避免 diagnostic tools 顯示沒有實際約束力的 uplink policy。

## Validity boundary

Application model 使用 `None` 表示 unavailable。UI、map、plot、state machine 與 CSV 不可重新把 `None` 轉成數字零。

一般 scalar 同時要求 v2 validity bit 成立且 wire value 不是 reserved sentinel。`error_code` 是完整 8-bit mask 的例外：valid bit 成立時 `0xFF` 是合法的 all-fault value；v1 沒有 validity flag，也保留完整 mask。

## Compatibility

下列 top-level modules只提供舊 import 相容：

- `gui_main.py`
- `serial_comm.py`
- `logger.py`
- `state_machine.py`
- `protocol.py`

Compatibility wrapper 不得新增 business logic。新功能放入 `rocket_ground_station/` 的對應 layer。

## Runtime data

`TelemetryCsvLogger` 只接受明確的 output directory。MainWindow 從 repository root 定位 `data/runs/`；此路徑是 ignored runtime output，不是 source 或 test fixture。

## Verification

```bash
PYTHONDONTWRITEBYTECODE=1 \
PYTHONPATH=apps/ground_station:protocol/python \
python3 -m unittest discover -s apps/ground_station/tests -q
```

GUI import smoke 應使用安裝 `requirements.txt` 的環境。純 application、transport 與 presentation tests 不需要 Qt display。
