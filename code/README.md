# Rocket 2027

Rocket 2027 是由 Rocket 2026 建立的獨立重整副本。Production firmware、operator app、shared protocol、diagnostics、data 與 legacy prototype 已分離；所有後續變更都遵守根目錄 [`AGENTS.md`](AGENTS.md)。

## 專案架構

```text
apps/ground_station/             production PyQt Ground Station
firmware/flight_computer/        production ESP32-S3 flight computer
firmware/ground_receiver/        production LoRa downlink receiver
firmware/camera/                 production OV5640 UART camera
protocol/                        canonical telemetry v1/v2 contract
diagnostics/                     diagnostic firmware and host tools
data/                            controlled samples and legacy run evidence
docs/                            architecture, safety and migration notes
archive/                         non-production legacy/unsafe prototypes
```

正式資料流：

```text
flight sensors
  -> navigation / flight state / ObserveOnly safety
  -> telemetry v2
       -> LoRa -> ground receiver -> USB/UART -> Ground Station
       -> onboard SD CSV
```

Telemetry v2 的唯一 wire-format 真源是 `protocol/schema/telemetry_v2.json`。Production code 不得依賴 `diagnostics/` 或 `archive/`。

## 安全狀態

Flight computer 是 **ObserveOnly** build：deployment criteria 可以被計算與記錄，但 GPIO16／5／6 不得驅動 servo、camera trigger 或 pyrotechnic output。舊 `fuse_cutter` 因具有 `ARM/FIRE` energized output，已隔離在 `archive/unsafe_actuation/`，不屬於 production build 或 verification。

完整規則見 [`docs/SAFETY.md`](docs/SAFETY.md)。

## 開發環境

- macOS 或其他可使用 PlatformIO Core 的環境。
- PlatformIO：`/Users/xuan/.platformio/penv/bin/pio`。
- Python 3，用於 protocol tests、Ground Station 與 diagnostic host tools。
- 燒錄前先執行 `pio device list` 找出實際 serial port，並關閉所有正在占用該 port 的程式。

## Build、test 與燒錄

從 repository root 執行：

```bash
python3 protocol/tools/generate_protocol.py --check
python3 protocol/tools/check_schema.py
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=protocol/python \
  python3 -m unittest discover -s protocol/tests -q
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=apps/ground_station:protocol/python \
  python3 -m unittest discover -s apps/ground_station/tests -q
/Users/xuan/.platformio/penv/bin/pio test \
  -d firmware/flight_computer -e native
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/flight_computer -e esp32-s3-devkitc-1-n16r8
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-uart0
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-usb-cdc
/Users/xuan/.platformio/penv/bin/pio run -d firmware/camera
git diff --check
```

確認 build 與接線後，以實際 port 取代 `/dev/cu.usbmodemXXXX`：

```bash
# Flight Computer: ESP32-S3, 16 MB flash
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/flight_computer -e esp32-s3-devkitc-1-n16r8 \
  -t upload --upload-port /dev/cu.usbmodemXXXX

# Ground Receiver: USB-UART bridge；若 PCB 使用 native USB，改用 usb-cdc environment
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-uart0 \
  -t upload --upload-port /dev/cu.wchusbserialXXXX

# OV5640 camera endpoint
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/camera -e esp32cam_ov5640 \
  -t upload --upload-port /dev/cu.usbserialXXXX
```

完整燒錄差異、BOOT mode 與 serial baud 請閱讀各 firmware README。Ground Receiver 與 Flight Computer 的 telemetry 是 binary stream，不應用一般文字 Serial Monitor 判讀。

Build output、virtual environments、Python cache 與 `data/runs/` 都由根目錄 `.gitignore` 排除。

## 文件

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)：全 repository 邊界與資料流。
- [`docs/SAFETY.md`](docs/SAFETY.md)：ObserveOnly 與 uplink safety contract。
- [`docs/MIGRATION.md`](docs/MIGRATION.md)：舊路徑到新路徑對照。
- [`firmware/flight_computer/README.md`](firmware/flight_computer/README.md)：flight build、pin map 與 modules。
- [`firmware/ground_receiver/README.md`](firmware/ground_receiver/README.md)：receiver environment、燒錄與 LED 狀態。
- [`firmware/camera/README.md`](firmware/camera/README.md)：camera pin map、燒錄與 UART contract。
- [`apps/ground_station/README.md`](apps/ground_station/README.md)：Ground Station 操作與測試。
- [`protocol/README.md`](protocol/README.md)：v1/v2 contract、generated files 與 drift gate。
- [`diagnostics/README.md`](diagnostics/README.md)：診斷工具索引。
