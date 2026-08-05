# Host Diagnostic Tools

這些工具透過 repository-root discovery 重用 Ground Station serial/presentation components，不複製 protocol 或 transport code。它們是 diagnostic interface，不是 production Ground Station，也不授權 production uplink。

從 repository root：

```bash
cd diagnostics/host_tools
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

Launchers:

| Command | Purpose |
| --- | --- |
| `.venv/bin/python barometer_main.py` | barometer/vertical-speed display and log |
| `.venv/bin/python gps_drift_main.py` | GPS drift/map display and log |
| `.venv/bin/python imu_vertical_accel_main.py` | vertical acceleration display and log |
| `.venv/bin/python lora_link_main.py` | LoRa link quality/loss display and log |
| `.venv/bin/python camera_uart_main.py` | OV5640 UART status/capture monitor |
| `.venv/bin/python telemetry_serial_dump.py` | terminal telemetry dump |

The shared `SerialPanel` hides production connection-mode policy in diagnostic windows because these tools own their explicit test commands. Production uplink authorization remains in `apps/ground_station`.

## Serial 與資料規則

- 啟動前先用 `/Users/xuan/.platformio/penv/bin/pio device list` 找出 port，並關閉 PlatformIO monitor 或其他占用者。
- Ground Receiver／Flight telemetry 預設 115200 baud；camera runtime UART 是 921600 baud。
- `telemetry_serial_dump.py` 使用 Ground Station typed pipeline；invalid/sentinel 欄位必須維持 missing，不可顯示為假零。
- 暫時輸出寫入 repository `data/runs/`，不要寫回 firmware source directory。
- 測試紀錄可能含 GPS 位置；移到 `data/samples/` 或對外分享前必須檢查敏感資訊。

Host tool 顯示成功不等於 sensor calibration、RF range、camera throughput 或 flight hardware qualification 已完成。
