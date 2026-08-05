# Diagnostics

Diagnostics 是獨立硬體測試韌體與 host tools，不會被 production firmware linked。成功 build、燒錄或取得單項測試資料，只是 diagnostic evidence，不代表 flight qualification、完整接線、RF range、校正或電源完整性已通過。

## Firmware

| Path | Purpose | Flash layout | Serial baud |
| --- | --- | --- | ---: |
| `firmware/barometer` | BMP3XX pressure/altitude test | ESP32-S3, 8 MB | 115200 |
| `firmware/gps_drift` | GPS fix/drift test | ESP32-S3, 8 MB | 115200 |
| `firmware/i2c_scanner` | I2C address scan | ESP32-S3, 8 MB | 115200 |
| `firmware/lora_link/flight` | flight-side LoRa/GPS link generator | ESP32-S3, 16 MB | 115200 |
| `firmware/lora_link/ground` | ground-side LoRa link receiver/reporter | ESP32-S3, 16 MB | 115200 |

每個目錄都是獨立 PlatformIO project，environment 均為 `esp32-s3-devkitc-1-n16r8`。從 repository root build：

```bash
/Users/xuan/.platformio/penv/bin/pio run -d diagnostics/firmware/barometer
```

燒錄前用 `pio device list` 確認 port。這些 project 目前預設使用 `/dev/cu.wchusbserial*`；若實際 port 不同，以 `--upload-port` 明確覆寫：

```bash
/Users/xuan/.platformio/penv/bin/pio device list
/Users/xuan/.platformio/penv/bin/pio run \
  -d diagnostics/firmware/barometer -e esp32-s3-devkitc-1-n16r8 \
  -t upload --upload-port /dev/cu.wchusbserialXXXX
```

其他 diagnostic firmware 只需替換 `-d` 路徑。若自動下載失敗，依 ESP32-S3 板卡的 `BOOT`／`RESET` 流程進入 download mode。Diagnostic firmware 不得燒錄到已連接飛行作動器或火工能源的硬體。

## Host tools

PyQt 與 serial tools 位於 [`host_tools/`](host_tools/README.md)。暫時擷取資料放入 `data/runs/`；只有經過檢查與刻意保留的資料才可移到 `data/samples/`。
