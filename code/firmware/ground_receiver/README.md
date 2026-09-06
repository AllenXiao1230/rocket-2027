# Production Ground Receiver

Rocket 2027 正式 ESP32-S3 LoRa 下行接收器。

It uses the same radio settings as the flight computer and accepts only
telemetry v2 (`A5 5A`, 64 bytes, CRC-16/CCITT-FALSE). Every accepted frame is
written to the selected serial transport unchanged.

## Data path

```text
firmware/flight_computer -- LoRa 433 MHz --> firmware/ground_receiver
  -- UART/USB --> apps/ground_station
```

The default build uses hardware UART0 for compatibility with a USB-to-UART
bridge. An optional native USB CDC environment is available for boards wired
to the ESP32-S3 USB connector. Both transports are binary-only: firmware
diagnostics must never be printed to `Serial`.

## Radio settings

- Frequency: 433 MHz
- Bandwidth: 125 kHz
- Spreading factor: 9
- Coding rate: 4/6
- Sync word: `0x12`
- Preamble: 8 symbols
- LoRa payload CRC: enabled

A 64-byte v2 frame occupies roughly 452 ms at this PHY before host-side
processing. The receiver therefore treats sequence gaps as observable link
loss; it does not fabricate or retransmit telemetry.

## Ground receiver pins

| Signal | GPIO |
| --- | ---: |
| LoRa SCK | 9 |
| LoRa MISO | 10 |
| LoRa MOSI | 11 |
| LoRa NSS | 12 |
| LoRa DIO0 | 6 |
| LoRa DIO1 | 5 |
| LoRa RESET | 7 |
| WS2812 | 48 |

## Status LED

- Red: radio initialization failed
- Amber: radio ready, waiting for a valid production frame
- Green: valid frame received recently
- Magenta: incompatible or corrupt frame rejected

## Board 與 environment

| Environment | Serial transport | 使用時機 |
| --- | --- | --- |
| `esp32-s3-ground-receiver-uart0` | Hardware UART0 | 預設；外接或板載 USB-UART bridge |
| `esp32-s3-ground-receiver-usb-cdc` | ESP32-S3 native USB CDC | PCB 實際接到原生 USB connector 時 |

兩者使用 16 MB flash、QIO、460800 upload speed 與 115200 serial baud。必須依實際 PCB connector 選擇，不能只因 port 名稱看起來相似就任意切換。

## Build

```bash
# 從 repository root；預設 USB-UART bridge
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-uart0

# Native ESP32-S3 USB CDC
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-usb-cdc
```

## 燒錄

```bash
/Users/xuan/.platformio/penv/bin/pio device list

# USB-UART bridge 版本
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-uart0 \
  -t upload --upload-port /dev/cu.wchusbserialXXXX

# 或 native USB CDC 版本
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/ground_receiver -e esp32-s3-ground-receiver-usb-cdc \
  -t upload --upload-port /dev/cu.usbmodemXXXX
```

若自動下載失敗，可用 ESP32-S3 的 `BOOT` + `RESET` 方式進入 download mode。燒錄後開啟 Ground Station、選擇 receiver port、baud 設為 115200，connection type 保持 `LoRa receiver / downlink only`。

Serial transport 是純 binary passthrough，不可加入文字 debug print，也不適合用一般文字 Serial Monitor 驗證內容。

This receiver is downlink-only. RTC sync and other future uplink commands must
use a separately designed command protocol; they are not silently forwarded.
