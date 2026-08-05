# Rocket Ground Station

Rocket 2027 的 production 操作者介面。它接收 flight computer 或 LoRa ground receiver 的 binary telemetry，顯示有效量測、飛行狀態、鏈路診斷與任務時間，並把資料寫入 repository 的 `data/runs/`。

## 快速開始

從 repository root：

```bash
cd apps/ground_station
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python main.py
```

Runtime dependency preflight 會檢查 PyQt5、Qt WebEngine、Qt3D、matplotlib 與 pyserial；缺少套件時會顯示安裝提示後安全結束。

Ground Station 是 host application，不需要燒錄。插入 Ground Receiver 或 Flight Computer 後，可先用 `/Users/xuan/.platformio/penv/bin/pio device list` 確認 serial port；同一個 port 不可同時被 PlatformIO monitor 與 Ground Station 開啟。

## 連線與 uplink 安全

連線類型預設為 **LoRa receiver / unknown（downlink only）**。這個模式拒絕 RTC sync 與所有 uplink，避免把 receiver 的 binary-only serial 誤當成可寫入 flight computer 的 command channel。

只有操作員明確選擇 **Direct flight UART（uplink enabled）** 後，既有 `RTC_SYNC:<unix>\n` 命令才會送出。Flight-state upload 按鈕維持停用，直到另有 versioned command protocol、安全檢查與測試。

Serial write failure 會依序產生 `ERROR`、停止 worker、關閉 serial port，再產生 `DISCONNECTED`；UI 不會保留失效的 worker reference。

建議連線設定：

| Hardware path | Baud | Connection type |
| --- | ---: | --- |
| Flight -> LoRa -> Ground Receiver -> host | 115200 | `LoRa receiver / downlink only` |
| Flight UART -> host | 115200 | `Direct flight UART`，僅明確需要 RTC sync 時使用 |

## Telemetry contract

- v1：47 bytes、`55 AA`、XOR；只保留相容讀取。
- v2：64 bytes、`A5 5A`、version/length/message type、CRC-16/CCITT-FALSE。
- Canonical parser 位於 repository 的 `protocol/python/rocket_protocol`；本應用不維護第二份 wire decoder。
- v2 `valid_flags` 與 deterministic sentinel 共同保護量測；無效值在 application model 是 `None`，UI 顯示 `---`，CSV 留空。
- `error_code` 是 8-bit bitmask；有效旗標成立時 `0xFF` 代表八項故障全數成立，不是 missing value。
- `(boot_id, sequence)` 用於 session、loss、duplicate、out-of-order 與 wrap 統計。Duplicate/out-of-order frame 只更新診斷，不得覆寫 UI、CSV 或 MissionClock。
- 主要高度優先使用有效 barometer altitude，否則 fallback 到有效 GPS altitude。

完整 wire contract 見 `../../protocol/telemetry_v2.md` 與 `../../protocol/schema/telemetry_v2.json`。

## 程式結構

```text
rocket_ground_station/
├── transport/       serial discovery、thread/callback byte transport
├── application/     typed snapshot、pipeline、mission clock、command policy、CSV
└── ui/              PyQt adapter 與 MainWindow
```

資料流固定為：

```text
serial bytes
  -> transport
  -> shared rocket_protocol parser
  -> application TelemetrySnapshot
  -> UI / CSV logger
```

`application` 與 `transport` 不 import PyQt。Top-level `gui_main.py`、`serial_comm.py`、`logger.py`、`state_machine.py`、`protocol.py` 是既有 import 的 compatibility wrappers；新功能應放入對應 layer。

更完整的邊界與失敗行為見 `ARCHITECTURE.md`。

## CSV

操作員啟動 logging 後，檔案建立於 repository root 的 `data/runs/`。Logger 使用固定欄位，記錄 protocol/session/sequence diagnostics 與工程值；`None` 寫成空欄，且不覆寫同 timestamp 的既有檔案。Write、flush、close error 都會回報，不可靜默忽略。

`data/runs/` 是 runtime output，不提交版本控制。

## 驗證

從 repository root 執行：

```bash
PYTHONDONTWRITEBYTECODE=1 \
PYTHONPATH=apps/ground_station:protocol/python \
python3 -m unittest discover -s apps/ground_station/tests -q
```

目前涵蓋 protocol adapter、mixed v1/v2 pipeline、validity/sentinel、session/sequence、stale-frame filter、MissionClock、command policy、serial lifecycle、port discovery、CSV 與 headless UI presentation。

## 顯示規則

- 尚未收到資料、無效狀態或 disconnect 後，飛行狀態顯示 `UNKNOWN`。
- 無效 GPS/barometer speed/altitude、battery、attitude、water 與 satellite data 不得顯示假零。
- 真實量測 `0` 仍是有效值，會正常顯示。
- 地圖與曲線只接受有效、有限數值。

Leaflet map 需要網路下載圖磚；圖磚不可用時不影響 telemetry、診斷與 logging。

硬體測試 GUI 已移至 `../../diagnostics/host_tools/`，production app 不再混放 diagnostic entry points。
