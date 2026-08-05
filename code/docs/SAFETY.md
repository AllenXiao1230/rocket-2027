# Rocket 2027 Safety Contract

## ObserveOnly flight build

目前 production flight firmware 只觀察 deployment criteria。`ActuatorController` 沒有 GPIO/PWM/fire backend；hardware interlock 永遠包含 compile-time `kInhibitObserveOnlyBuild`，caller 或 runtime input 不能清除。

下列 pins 在 boot、reset 與 loop 都必須保持 inactive：

| Function | GPIO | Production status |
| --- | ---: | --- |
| Separation servo | 16 | inactive / ObserveOnly |
| Camera A trigger | 5 | inactive / ObserveOnly |
| Camera B trigger | 6 | inactive / ObserveOnly |

GPIO21 只保留 legacy safety-switch classification，不是 arm 訊號。

## Command boundary

- Ground Receiver 是 binary downlink-only transport，不轉送 RTC 或其他 uplink。
- Ground Station 對 unknown/receiver link 預設拒絕所有 uplink。
- 只有操作員明確選擇 direct flight UART 時，既有 RTC sync 才可送出。
- Flight-state upload 維持停用，直到另有 versioned command protocol、authorization、interlock 與 HIL evidence。

## Archived energized prototype

`archive/unsafe_actuation/fuse_cutter` 含有 `ARM`、`FIRE` 與 1.5 秒高電位輸出。它只為歷史保存：

- 不屬於 Rocket 2027 production firmware；
- 不出現在 root workspace；
- 不被 production build/test 呼叫；
- 不可在已接能源或火工品的硬體上執行。

若未來要恢復實體作動，必須另有硬體設計、雙通道 arm、continuity、energy-bus feedback、timeout、fault response、HIL 與獨立 safety review；不得直接沿用 archive prototype。

## Verification meaning

Compile/unit-test success 只證明 software contract。它不等於 wiring、RF range、sensor calibration、power integrity、camera throughput 或 deployment hardware 已完成驗證。
