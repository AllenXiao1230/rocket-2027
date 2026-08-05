# Archive

Everything under this directory is **non-production** and retained only for history/reference.

- `firmware/camera_wifi_prototype`: earlier Wi-Fi camera experiment with embedded credentials in legacy source.
- `firmware/flight_computer_stub`: unused PlatformIO stub.
- `diagnostics/packet_loss_v1`: superseded sender/receiver packet-loss pair.
- `ground_station_legacy`: unused UI/model prototypes.
- `unsafe_actuation/fuse_cutter`: energized `ARM/FIRE` prototype; isolated by the Rocket 2027 safety contract.

Archive code is excluded from root workspace and verification commands. Do not add it to production include paths or dependencies.

## 使用規則

- 不要對 archive project 執行 production build、upload 或 release 流程。
- 歷史 README、pin、protocol、credential 與驗證說明可能已失效。
- 若需要取回概念，應先移植到 current canonical component、補上測試與安全審查，而不是讓 production code 直接依賴 archive。
- `unsafe_actuation/` 禁止燒錄或在連接能源的硬體上執行。
- `camera_wifi_prototype/` 含 legacy hard-coded credentials，禁止部署或分享未清理 source。

目前正式入口請回到 repository 根目錄 [`../README.md`](../README.md)。
