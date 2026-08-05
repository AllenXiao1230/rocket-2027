# Rocket 2027 Path Migration

本表記錄第 7 項重整，供既有 command、IDE workspace 與文件更新。

| Previous path | Current path | Classification |
| --- | --- | --- |
| `rocket_main/` | `firmware/flight_computer/` | production |
| `ground_station/` | `apps/ground_station/` | production app |
| `rocket_cam_ov5640/` | `firmware/camera/` | production |
| `rocket_baro_altitude_test/` | `diagnostics/firmware/barometer/` | diagnostic |
| `rocket_gps_drift_test/` | `diagnostics/firmware/gps_drift/` | diagnostic |
| `rocket_i2c_scan_test/` | `diagnostics/firmware/i2c_scanner/` | diagnostic |
| `rocket_lora_link_test/` | `diagnostics/firmware/lora_link/flight/` | diagnostic |
| `ground_lora_link_test/` | `diagnostics/firmware/lora_link/ground/` | diagnostic |
| Ground Station test GUIs | `diagnostics/host_tools/` | diagnostic |
| `test data/` | `data/samples/` | controlled sample data |
| `Rocker-cam/` | `archive/firmware/camera_wifi_prototype/` | legacy |
| `rocket_main2/` | `archive/firmware/flight_computer_stub/` | legacy |
| `ground_terminal/` + `rocket_packet_loss_test/` | `archive/diagnostics/packet_loss_v1/` | legacy diagnostic |
| `fuse_cutter/` | `archive/unsafe_actuation/fuse_cutter/` | unsafe legacy |

Production commands and `rocket.code-workspace` now use only the current paths. Historical documentation is retained under `docs/legacy/` and may intentionally contain old names.
