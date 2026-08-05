# OV5640 UART Camera

Production camera endpoint for an OV5640/ESP32 camera target. It accepts newline commands over 921600-baud UART:

- `STATUS`
- `CAPTURE`
- `BURST START <interval_ms>`
- `BURST STOP`

Frames are returned as `CAM_FRAME` metadata, raw JPEG bytes and `CAM_FRAME_END`; status/errors remain text-framed. Matching host tool 是 [`../../diagnostics/host_tools/camera_uart_main.py`](../../diagnostics/host_tools/camera_uart_main.py)。

## Board 與設定

| Item | Value |
| --- | --- |
| PlatformIO board | `esp32cam` |
| Environment | `esp32cam_ov5640` |
| Partition | `huge_app.csv` |
| Upload speed | 115200 |
| Runtime UART | 921600 |

燒錄前必須檢查 [`src/camera_pins.h`](src/camera_pins.h) 是否符合實際 OV5640 module；錯誤 pin map 可能造成初始化失敗，不能以編譯成功代替接線確認。

## Build 與燒錄

從 repository root：

```bash
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/camera -e esp32cam_ov5640
/Users/xuan/.platformio/penv/bin/pio run \
  -d firmware/camera -e esp32cam_ov5640 \
  -t upload --upload-port /dev/cu.usbserialXXXX
```

若 ESP32-CAM 沒有自動下載電路，燒錄前將 GPIO0 接地並 reset／重新上電；upload 完成後移除 GPIO0 接地，再 reset 進入正常執行模式。請以實際板卡文件為準，不要在未知 pin/電源狀態下嘗試。

## Host 驗證

```bash
cd diagnostics/host_tools
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python camera_uart_main.py
```

選擇 camera serial port 與 921600 baud。JPEG payload 是 binary data，不應只用文字 terminal 判讀。
