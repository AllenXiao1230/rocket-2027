# Flight Computer

Rocket 2027 正式 ESP32-S3 飛行電腦韌體。`src/main.cpp` 負責 application orchestration；navigation、flight、telemetry 與 storage core 不依賴 Arduino，可在 native environment 測試。

## 安全狀態

目前是 **ObserveOnly** build。Deployment criteria 只會被計算、記錄與下傳，不存在 actuator hardware backend；GPIO16／5／6 必須保持 inactive。燒錄與測試時仍應斷開伺服器、點火器及其他有能量的作動裝置。完整規則見 [`../../docs/SAFETY.md`](../../docs/SAFETY.md)。

## Board 與 environment

| Item | Value |
| --- | --- |
| Board | ESP32-S3 DevKitC-1 compatible |
| PlatformIO environment | `esp32-s3-devkitc-1-n16r8` |
| Flash layout | 16 MB, `default_16MB.csv` |
| Flash mode | QIO |
| Upload speed | 460800 |
| Serial/telemetry baud | 115200 |
| Native test environment | `native` |

## Build 與測試

從 repository root 執行：

```bash
/Users/xuan/.platformio/penv/bin/pio test \
  -d firmware/flight_computer -e native
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/flight_computer -e esp32-s3-devkitc-1-n16r8
```

Native suite 涵蓋 flight safety/state、altitude/attitude estimation、battery scaling、telemetry mapping/shared encoding 與固定 schema CSV formatting。

## 燒錄

先列出 serial port，並關閉 Ground Station 或其他占用該 port 的程式：

```bash
/Users/xuan/.platformio/penv/bin/pio device list
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/flight_computer -e esp32-s3-devkitc-1-n16r8 \
  -t upload --upload-port /dev/cu.usbmodemXXXX
```

若自動下載失敗，按住 `BOOT`、按一下 `RESET`、放開 `BOOT` 後重新 upload。燒錄完成後再 reset。Flight UART 會輸出 binary telemetry v2；一般文字 monitor 可能顯示亂碼，應由 `apps/ground_station` 或 typed diagnostic tool 解析。

## 模組與資料流

```text
board/drivers -> engineering values + validity
  -> navigation estimators
  -> flight state + ObserveOnly interlock
  -> TelemetrySample
       -> v2 frame -> LoRa + UART
       -> storage record -> SD CSV
```

依賴細節見 [`ARCHITECTURE.md`](ARCHITECTURE.md)。Telemetry wire format 只能來自 [`../../protocol/`](../../protocol/README.md)。

## Pin ownership

The canonical pin map is `src/board/pin_map.h`.

| Function | GPIO |
| --- | --- |
| I2C A SDA / SCL | 20 / 19 |
| I2C B SDA / SCL | 9 / 10 |
| GPS TX / RX / PPS | 35 / 45 / 47 |
| Sub UART TX / RX | 17 / 18 |
| LoRa SCK / MISO / MOSI / NSS | 42 / 41 / 40 / 39 |
| LoRa RESET / DIO0 / DIO1 | 38 / 37 / 36 |
| SD CS / MOSI / SCK / MISO | 11 / 12 / 13 / 14 |
| Buzzer / WS2812 | 8 / 48 |
| Battery ADC / RTC SQW / water | 4 / 3 / 15 |
| Legacy safety switch | 21 |
| ObserveOnly outputs | 16 / 5 / 6, inactive |

LoRa production settings are 433 MHz, BW 125 kHz, SF9, coding rate 4/6, sync word `0x12`, preamble 8 and payload CRC enabled.

舊 Rocket 2026 長篇實作筆記保存在 [`../../docs/legacy/flight_computer_2026.md`](../../docs/legacy/flight_computer_2026.md)，不是目前的 contract。
