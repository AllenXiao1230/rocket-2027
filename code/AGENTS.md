# Rocket 2027 — Agent Working Agreement

本檔案適用於整個 repository。任何人或自動化 agent 修改本專案前，都必須先閱讀本檔案。

## 不可破壞的邊界

- 所有開發、搬移、清理與測試只可發生在 `rocket 2027/code` 副本。
- 不得修改 `/Users/xuan/Documents/rocket2026/code` 原始專案。
- 不得將 `.pio/`、`.venv/`、`__pycache__/`、`.DS_Store`、執行期 CSV／log 提交進版本控制。
- 不得使用會清除使用者未提交內容的 destructive Git 指令。
- 若功能行為、wire protocol、硬體 pin、狀態機、安全規則、建置方式或目錄責任有變更，必須在同一批變更中更新本檔案的「功能變更紀錄」。

## 安全邊界

- Rocket 2027 飛控目前是 **ObserveOnly**：狀態機可以判定 deployment criteria，但不得驅動 GPIO、PWM、servo 或 pyrotechnic output。
- GPIO21 只保留 legacy safety classification，不是 arm 訊號。
- `kInhibitObserveOnlyBuild` 必須永遠存在，且不得由 runtime input 清除。
- `ActuatorController` 不得增加 `fire()`、硬體 backend、GPIO、PWM 或能量輸出 API，除非另有硬體設計、雙通道 arm、continuity、energy-bus feedback 與 HIL 證據，並經獨立審查。
- GPIO16／5／6 在 boot、reset 與 loop 中必須維持 inactive。

## Canonical architecture

```text
apps/                  操作者應用程式；UI 不負責解析 wire bytes
firmware/              可部署的 embedded firmware
protocol/              telemetry schema、產生器、C++/Python library、golden vectors
diagnostics/           感測器、鏈路與工具型診斷
data/                  受控測試資料與 sample；runtime output 不提交
docs/                  架構、接線、安全與操作文件
archive/               不再維護的 prototype／legacy 實作
```

上述目錄是完成後的 canonical layout。Production 路徑固定為 `firmware/flight_computer/`、`firmware/ground_receiver/`、`firmware/camera/` 與 `apps/ground_station/`。

## Dependency rules

- `protocol/schema/telemetry_v2.json` 是 telemetry v2 的唯一 wire-format 真源。
- Python 與 C++ generated constants 必須由 `protocol/tools/generate_protocol.py` 產生；不可手工複製 offset、flag 或 CRC 參數。
- Ground Station 必須使用共用 `protocol/python/rocket_protocol`，不可維護第二份 parser。
- Flight firmware 的依賴方向為：

```text
application/main
  -> drivers
  -> navigation
  -> flight
  -> telemetry
  -> storage
```

- `flight` 與 navigation 純核心不得依賴 Arduino、LoRa、SD、PyQt 或 GUI。
- telemetry 只建立 immutable frame；LoRa／UART 屬於 transport/driver。
- Ground Station 的依賴方向為 `transport -> protocol -> application -> UI`；application 不得 import PyQt。
- Production code 不得依賴 `diagnostics/` 或 `archive/`；`archive/unsafe_actuation/` 不得加入 workspace、include path 或 build script。

## Documentation contract

- 根目錄 `README.md` 必須提供目前 canonical layout、production data flow、驗證入口與三個 production firmware 的燒錄入口。
- 每個 deployable firmware README 必須列出 board/environment、build、upload、serial baud、pin/configuration prerequisite 與安全限制。
- `protocol/README.md` 必須指出 schema 真源、generated files、相容性與 drift gate；consumer README 不可另寫一份相互矛盾的 wire contract。
- `diagnostics/README.md` 與 `diagnostics/host_tools/README.md` 必須明確標示 diagnostic evidence 不等於 flight qualification。
- `archive/` 下每一份 README 都必須標示 non-production；unsafe actuation 與含 credentials 的 prototype 必須額外標示禁止部署。
- 文件內的命令預設從 repository root 執行；若必須切換目錄，要在同一段明示。
- 功能、pin、environment、baud、路徑、資料格式或安全規則變更時，相關 README 與本檔案必須在同一批變更更新。

## Protocol contract

- telemetry 僅有 v2：64 bytes，sync `A5 5A`，version/length/message type header，little-endian，CRC-16/CCITT-FALSE。
- Flight Computer 的 LoRa 與 UART 必須傳送相同的 v2 frame；Ground Receiver 與 Ground Station 只接受合法 v2 frame。
- v2 `valid_flags` 未設時，對應欄位必須使用 deterministic sentinel；UI、地圖、state 與 CSV 不得把 sentinel 當成量測值。
- `error_code` 是 8-bit bitmask 例外：`VALID_ERROR_CODE` 成立時 `0xFF` 代表八項 fault 全部成立；valid bit 未設時才是 missing。
- `boot_id + sequence` 用於 session、loss、duplicate 與 out-of-order 統計。
- Ground Station 對 duplicate/out-of-order v2 frame 只更新 diagnostics，不得覆寫 UI、CSV 或 MissionClock。
- Ground Station 未明確辨識的 serial link 必須 fail closed 為 LoRa receiver/downlink-only；只有操作員明確選擇 direct flight UART 才可傳 RTC sync。
- Flight-state upload 維持停用，直到有 versioned command protocol 與對應安全測試。

## Build and verification

在 repository root 執行：

```bash
python3 protocol/tools/generate_protocol.py --check
python3 protocol/tools/check_schema.py
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=protocol/python \
  python3 -m unittest discover -s protocol/tests -q
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=apps/ground_station:protocol/python \
  python3 -m unittest discover -s apps/ground_station/tests -q
/Users/xuan/.platformio/penv/bin/pio test -d firmware/flight_computer -e native
/Users/xuan/.platformio/penv/bin/pio run -d firmware/flight_computer \
  -e esp32-s3-devkitc-1-n16r8
/Users/xuan/.platformio/penv/bin/pio run -d firmware/ground_receiver \
  -e esp32-s3-ground-receiver-uart0
/Users/xuan/.platformio/penv/bin/pio run -d firmware/ground_receiver \
  -e esp32-s3-ground-receiver-usb-cdc
/Users/xuan/.platformio/penv/bin/pio run -d firmware/camera
git diff --check
```

燒錄必須使用對應 environment，並以 `pio device list` 確認 port；不得依檔名或上次使用結果猜測硬體：

```bash
/Users/xuan/.platformio/penv/bin/pio run -d firmware/flight_computer \
  -e esp32-s3-devkitc-1-n16r8 -t upload --upload-port /dev/cu.usbmodemXXXX
/Users/xuan/.platformio/penv/bin/pio run -d firmware/ground_receiver \
  -e esp32-s3-ground-receiver-uart0 -t upload --upload-port /dev/cu.wchusbserialXXXX
/Users/xuan/.platformio/penv/bin/pio run -d firmware/camera \
  -e esp32cam_ov5640 -t upload --upload-port /dev/cu.usbserialXXXX
```

## 變更本檔案的規則

每一項功能更動都要：

1. 更新受影響的架構、contract 或驗證命令。
2. 在下方新增一筆 `YYYY-MM-DD` 紀錄，說明「做了什麼、為什麼、如何驗證」。
3. 若只是 formatting、註解拼字或無行為的純搬移，可合併成同一筆紀錄。

## 功能變更紀錄

### 2026-07-30

- 建立 Rocket 2027 獨立副本；所有重整工作限制在副本內。
- 新增 production `firmware/ground_receiver`：使用與飛控一致的 LoRa PHY，驗證 v1/v2 application frame 後，以 binary-only UART/USB serial passthrough。
- 將 flight state machine、deployment criteria 與 actuator safety boundary 分離；加入 apogee hold、fallback 與 native regression tests。Actuator 維持 ObserveOnly。
- 建立 telemetry v2 canonical JSON schema、Python/C++ shared libraries、CRC-16、valid/source flags、boot ID、sequence、mixed-stream parser、golden vector與跨語言測試；flight 預設送 v2，receiver 同時接收 v1/v2。
- 新增 schema generator 與 `--check` drift gate；generated constants 現在包含完整 wire offsets，Python/C++ 預設不再把 sentinel 宣告為有效欄位。
- GPS position、altitude、speed、satellite count 與 course 改成獨立 validity/timeout；encode 失敗時不送出 frame 且不增加 sequence。已以 ESP32-S3 production build 驗證。
- Ground receiver 提供 UART0 預設與 native USB CDC 選用環境；兩個 environment 均已成功建置，實機須依 PCB connector 選擇。
- 修正 ObserveOnly interlock，使 `kInhibitObserveOnlyBuild` 無法被 caller-controlled runtime input 清除。
- 初始化本 `AGENTS.md`；後續所有功能更動必須同步記錄。
- 將飛控拆成 board pin map、battery/LoRa drivers、navigation、flight、telemetry 與 storage 邊界；`main.cpp` 只保留應用層協調與硬體 adapter。
- 抽出 altitude 與 attitude estimator：保留 barometer/GPS fallback、vertical-speed filter、Madgwick、yaw zero/hold 與 wrap-safe 時間差，並以 native tests 固定既有行為。
- 建立單一 `TelemetrySample -> TelemetryBuilder` validity/scaling 路徑；未配置實體回授的 servo 欄位保持 invalid，不再輸出假零值。
- 建立固定 26 欄、拒絕截斷的 CSV formatter 與 SD adapter；invalid/non-finite 欄位留空，write/flush failure 會使 storage health fail closed。第 4 點以 43 個 native tests 與 ESP32-S3 production build 驗證。
- 將 Ground Station 拆成 UI-independent `transport`、canonical protocol adapter、pure `application` 與 PyQt `ui`；保留 top-level compatibility wrappers，新增 architecture/operation 文件。
- Ground Station 現支援 mixed v1/v2、typed optional snapshot、boot/sequence diagnostics、stale-frame suppression、MissionClock 與固定 CSV；invalid/no-data 顯示 `UNKNOWN` 或 `---`，不再製造假零。`error_code=0xFF` 會保留完整 fault mask。
- Ground link 預設 downlink-only，diagnostic tools 不顯示無效的 link-policy selector；state upload 停用。Serial write failure 依序 `ERROR -> stop -> close -> DISCONNECTED`，UI 先停止 worker 再清 reference。第 5 點以 44 個 ground tests、protocol schema/tests 與 GUI import smoke 驗證。
- 新增 repository root `.gitignore`，清除副本中已追蹤或未追蹤的 `.pio`、`.venv`、`__pycache__`、bytecode、`.DS_Store`、runtime telemetry/diagnostic logs 與 patch leftovers；後續 build、test 與 operator run 只能產生 ignored artifacts。

### 2026-08-05

- 完成 canonical directory migration：production 移至 `firmware/`／`apps/`，硬體與 host 測試移至 `diagnostics/`，受控記錄移至 `data/`，歷史與不安全 prototype 移至 `archive/`，跨專案文件集中到 `docs/`。
- 將具有 energized `ARM/FIRE` 的 fuse-cutter prototype 隔離到 `archive/unsafe_actuation/` 並自 workspace/build 移除；Rocket 2027 production safety 仍維持 ObserveOnly。
- 修正 flight `lib_extra_dirs`、protocol schema checker、Ground Station shared-protocol root discovery、tests、diagnostic host imports、VS Code workspaces 與所有 current build/test 文件，使其不依賴 working directory 或舊路徑。
- Diagnostic telemetry terminal 改用 Ground Station typed pipeline，invalid/sentinel 欄位不再以工程數值輸出；這是與 production UI 相同的 validity boundary。
- Camera firmware 改用目前 ESP32 camera framework 的 `pin_sccb_sda`／`pin_sccb_scl` 欄位，移除 deprecated SCCB pin API warning；UART command/frame contract 不變。
- 完成最終驗證：schema/generated/golden-vector drift gate 通過；protocol 6/6、Ground Station 44/44、flight native 43/43 tests 通過；flight、ground receiver UART0／USB CDC、camera 與五個 diagnostic firmware 均成功建置。另通過 67 個 Python source AST、3 個 JSON、`git diff --check`、production dependency boundary 與 generated-artifact cleanliness 檢查；所有建置產物均在驗證後自副本移除。

### 2026-08-05 — 文件同步

- 依目前 canonical layout、PlatformIO environments 與安全 contract 全面更新 README；新增 root-level build/upload 入口、各 production target 的 port/BOOT/baud 說明、diagnostic 燒錄規則與 protocol README。
- 修正本檔案的原始專案唯讀路徑為 `/Users/xuan/Documents/rocket2026/code`，並新增 documentation contract，要求未來功能、pin、environment、baud、protocol、路徑與安全規則變更時同步更新 README 與 AGENTS。
- Archive 內的 PlatformIO boilerplate README 改為 non-production 警告，避免歷史 stub 被誤認為可部署目標；本次只變更文件，不改程式行為。

### 2026-08-05 — telemetry v2-only

- 移除 production protocol library、Ground Receiver、Ground Station parser、CSV diagnostics 與測試中的 v1 encode/decode／mixed-stream 相容路徑；LoRa 與 Flight UART 均維持傳送同一個 64-byte v2 frame，Ground Receiver 與 Ground Station 只接受該 frame。
- 更新 generated constants、wire contract、operator／firmware 文件與本規範，避免 v1 仍被誤認為可部署通訊格式。驗證須通過 schema drift gate、protocol／Ground Station regression、flight native tests、Ground Receiver production builds 與 `git diff --check`。
