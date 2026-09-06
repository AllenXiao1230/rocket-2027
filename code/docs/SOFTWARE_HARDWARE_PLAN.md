# Rocket 2027 軟硬體統整規劃

> 本文件統整目前 production repository 已實作的軟硬體設計與下一階段驗證項目。
> 它描述的是目前程式與文件可確認的範圍；未在此明示的電源、PCB 接線、感測器型號與火箭機構，不應由本文件推定。

## 1. 系統目標與安全狀態

系統的核心目標是：在飛行端蒐集感測資料、在機載端保存資料，並以 LoRa 將一致的 telemetry 送到地面站即時顯示與記錄。

目前飛行電腦是 **ObserveOnly**：可以估測飛行狀態與 deployment criteria，但不會驅動任何伺服、相機 trigger、點火器或其他有能量的作動器。GPIO 16、5、6 在 boot、reset 與正常 loop 都必須維持 inactive。未來若要導入實體作動，必須另行完成硬體設計、雙通道 arm、continuity、energy-bus feedback、timeout/fault response、HIL 測試與獨立 safety review；不得直接從目前程式或 archive prototype 啟用。

## 2. 整體架構

```text
                         ┌────────────────────────────────┐
                         │ Flight Computer (ESP32-S3)       │
                         │ sensors → navigation/state       │
                         │       → telemetry v2             │
                         │       ├─ SD CSV (機載記錄)        │
                         │       ├─ LoRa 433 MHz             │
                         │       └─ UART (直接連線選用)      │
                         └───────────────┬────────────────┘
                                         │ LoRa telemetry v2
                                         ▼
                         ┌────────────────────────────────┐
                         │ Ground Receiver (ESP32-S3)       │
                         │ LoRa 驗證 → binary UART/USB      │
                         └───────────────┬────────────────┘
                                         │ 115200 baud
                                         ▼
                         ┌────────────────────────────────┐
                         │ Ground Station (macOS/PC)        │
                         │ parser → application → PyQt UI   │
                         │                 └─ host CSV      │
                         └────────────────────────────────┘

              Independent camera endpoint: OV5640 / ESP32-CAM ↔ UART 921600
```

所有 production telemetry 都是同一份 v2 binary frame：64 bytes、`A5 5A` sync、little-endian、CRC-16/CCITT-FALSE。飛行電腦對 LoRa 與 UART 送出的 frame 必須完全相同；Ground Receiver 與 Ground Station 僅接受通過 v2 驗證的資料。

## 3. 硬體規劃

### 3.1 飛行電腦

| 項目 | 已定義規劃 |
| --- | --- |
| 主控 | ESP32-S3 DevKitC-1 compatible |
| 韌體環境 | `esp32-s3-devkitc-1-n16r8`；16 MB QIO flash、`default_16MB.csv` |
| 感測／運算 | 感測器資料進入 navigation estimators、flight state 與 ObserveOnly criteria；純核心可在 host native tests 執行 |
| 下傳 | LoRa 433 MHz + UART，均為 telemetry v2 binary stream，115200 baud |
| 機載記錄 | SD CSV；無效或 non-finite 欄位保留空白，storage write/flush failure 應 fail closed |
| 狀態提示 | Buzzer GPIO8、WS2812 GPIO48 |
| 安全 | Legacy safety switch GPIO21 只做分類，不是 arm 信號；GPIO16/5/6 永不輸出 |

飛行電腦 pin ownership：

| 功能 | GPIO |
| --- | --- |
| I2C A SDA / SCL | 20 / 19 |
| I2C B SDA / SCL | 9 / 10 |
| GPS TX / RX / PPS | 35 / 45 / 47 |
| Sub UART TX / RX | 17 / 18 |
| LoRa SCK / MISO / MOSI / NSS | 42 / 41 / 40 / 39 |
| LoRa RESET / DIO0 / DIO1 | 38 / 37 / 36 |
| SD CS / MOSI / SCK / MISO | 11 / 12 / 13 / 14 |
| Battery ADC / RTC SQW / water | 4 / 3 / 15 |
| ObserveOnly outputs | 16 / 5 / 6（inactive） |

### 3.2 無線鏈路與地面接收器

飛行端與 Ground Receiver 使用相同 LoRa PHY：433 MHz、125 kHz bandwidth、SF9、coding rate 4/6、sync word `0x12`、preamble 8、LoRa payload CRC enabled。64-byte v2 frame 的空中傳輸時間約 452 ms；因此 sequence gap 是可觀測的鏈路遺失，不由 receiver 偽造資料或重傳。

| Ground Receiver 項目 | 已定義規劃 |
| --- | --- |
| 主控 | ESP32-S3，16 MB QIO flash |
| LoRa 接腳 | SCK/MISO/MOSI/NSS: 9/10/11/12；DIO0/DIO1/RESET: 6/5/7 |
| Host transport | 預設 UART0 USB-UART bridge；可選 ESP32-S3 native USB CDC |
| Host baud | 115200，binary-only passthrough，禁止 serial debug print |
| 狀態 LED | WS2812 GPIO48：紅=radio init failed、琥珀=等待合法 frame、綠=近期收到、洋紅=拒絕錯誤／不相容 frame |
| 安全 | 僅下行；不轉送 RTC sync 或任何未來 uplink command |

### 3.3 相機端點

| 項目 | 已定義規劃 |
| --- | --- |
| 硬體 | OV5640 / ESP32 camera target |
| 韌體環境 | `esp32cam_ov5640`，`huge_app.csv` |
| UART | 921600 baud |
| 命令 | `STATUS`、`CAPTURE`、`BURST START <interval_ms>`、`BURST STOP` |
| 回傳 | `CAM_FRAME` metadata + raw JPEG + `CAM_FRAME_END`；status/error 為 text-framed |
| 接線前提 | 必須以實體 OV5640 module 核對 `src/camera_pins.h`；編譯成功不代表 pin map 或供電正確 |

相機端點目前是獨立 UART protocol，並不屬於 telemetry v2 或 Ground Receiver passthrough 路徑。

## 4. 軟體規劃

| 層級 | 元件 | 責任與限制 |
| --- | --- | --- |
| Shared protocol | `protocol/` | `schema/telemetry_v2.json` 是 wire-format 唯一真源；產生 C++/Python constants、encoder/parser 與 golden vectors |
| Flight firmware | `firmware/flight_computer/` | board/drivers → navigation → flight → telemetry/storage；`main.cpp` 只協調 application 與 hardware adapter |
| Ground receiver firmware | `firmware/ground_receiver/` | 驗證 v2 frame 後原樣輸出；不解析成文字、不做重傳、不接受上行 |
| Camera firmware | `firmware/camera/` | 管理 camera sensor、UART command 與 JPEG binary transfer |
| Ground Station | `apps/ground_station/` | `transport → shared protocol → application → PyQt UI`；UI 不解析 wire bytes，application/transport 不 import PyQt |
| 資料記錄 | Flight SD CSV、Ground Station `data/runs/` | 無效值保持 empty，runtime output 不提交版本控制 |
| 診斷與歷史 | `diagnostics/`、`archive/` | 僅供診斷或保存；production code 不得依賴它們 |

Ground Station 對缺失資料使用 `None`、UI 顯示 `---`、CSV 留空，而非假零。`boot_id + sequence` 用於辨識 session、loss、duplicate 與 out-of-order；duplicate/out-of-order frame 只更新 diagnostics，不得改寫 UI、MissionClock 或 CSV。預設連線類型為 receiver/unknown downlink-only；僅操作員明確選擇 direct flight UART 時，既有 RTC sync 才能上行。Flight-state upload 仍停用。

## 5. 已具備與待完成項目

| 狀態 | 項目 | 說明 |
| --- | --- | --- |
| 已具備 | 端到端 telemetry v2 | Flight、LoRa、Ground Receiver、UART/USB、Ground Station 共享 v2 contract |
| 已具備 | 安全軟體邊界 | ObserveOnly interlock、downlink-only receiver、unknown link fail closed |
| 已具備 | 主機與機載記錄 | SD CSV 與 Ground Station CSV，無效值不偽造 |
| 已具備 | 三個 production firmware 入口 | Flight Computer、Ground Receiver、Camera 均有 PlatformIO environment 與燒錄文件 |
| 待實機驗證 | PCB 接線與電源完整性 | 各 pin map、sensor bus、LoRa、SD、GPS、camera 與供電需依實體板件驗證；本 repo 的 compile/test 不代表完成 |
| 待實機驗證 | RF 鏈路品質 | 頻率合法性、天線、距離、姿態遮蔽、封包遺失率與現場干擾需以量測確認 |
| 待實機驗證 | 感測器校正與飛行演算法 | 高度、姿態、GPS、battery、水感測器的零點、尺度、timeout 與飛行條件需以地面／HIL／飛測確認 |
| 待實機驗證 | SD 與相機吞吐 | 寫入持續性、掉電行為、JPEG 帶寬、UART framing 及供電壓降需量測 |
| 未啟用 | 實體 deployment/pyro | 需要新的獨立安全設計與 HIL 證據，不能直接開啟現有 GPIO |
| 未啟用 | Versioned uplink command protocol | 除 direct-flight RTC sync 外，尚未設計 authorization/interlock/acknowledgement 與測試 |

## 6. 建議驗證順序

1. **桌面回歸**：先通過 schema drift gate、protocol tests、Ground Station tests、flight native tests 與三個 firmware build。
2. **單板 bring-up**：逐一確認 Flight Computer、Ground Receiver、Camera 的正確 board、port、BOOT mode、供電與 pin map；binary stream 以 Ground Station/typed tool 驗證，不以文字 serial monitor 判讀。
3. **有線整合**：Flight UART → Ground Station，確認 v2 parsing、validity、CSV、RTC sync policy 與斷線行為。
4. **無線整合**：Flight LoRa → Ground Receiver → Ground Station，量測 sequence loss、延遲與 receiver status LED，確認 receiver 仍為純下行。
5. **機載持續測試**：SD 寫入、GPS、感測器、電池與相機在目標供電、震動與操作時間下的穩定性。
6. **飛行前審查**：以實測證據核對 PCB 接線、電源、RF、感測器校正、機構與 safety contract；不得將 compile/test 當成飛行資格。

## 7. 文件與真源

- 系統架構：`docs/ARCHITECTURE.md`
- 安全 contract：`docs/SAFETY.md`
- Telemetry 真源：`protocol/schema/telemetry_v2.json`
- Flight pin map：`firmware/flight_computer/src/board/pin_map.h`
- 個別元件操作：各 production directory 下的 `README.md`

本文件不取代上述真源；若 hardware pin、wire protocol、狀態機、安全規則或建置方式變更，必須同步更新對應文件與 `AGENTS.md` 的功能變更紀錄。
