# Flight computer architecture

The firmware is split into hardware-independent `RocketCore` logic and
ESP32/Arduino adapters. `src/main.cpp` currently remains the boot/application
orchestrator while hardware services are migrated out incrementally.

```text
src/main.cpp
├── board/pin_map.h
├── drivers/
│   ├── battery_monitor
│   └── lora_transmitter
├── storage/sd_telemetry_store
└── lib/rocket_core/src/
    ├── model/
    ├── navigation/ (altitude + attitude estimators)
    ├── telemetry/
    └── storage/

include/flight/
├── flight_state_machine
├── hardware_interlock
└── actuator_controller
```

## Dependency direction

- Board/drivers may depend on Arduino and device libraries.
- Navigation, flight, telemetry mapping, and CSV formatting must remain
  hardware-independent and native-testable.
- Flight state logic consumes values and validity; it does not read sensors.
- Telemetry mapping produces `TelemetryV2Fields`; the shared protocol library
  alone owns endian encoding and CRC.
- Storage formatting does not own an SD card. `SdTelemetryStore` is the thin
  Arduino adapter that writes a complete formatted row or fails without
  truncation.

## Loop data flow

```text
sensor drivers
  -> engineering-unit values + validity
  -> navigation estimates
  -> flight state machine
  -> ObserveOnly actuator boundary
  -> one TelemetrySample
       ├── TelemetryBuilder -> one immutable v2 frame -> LoRa + UART
       └── TelemetryStorageRecord -> CsvFormatter -> SD
```

The same `time_ms` captured at the top of `loop()` is used throughout the
pipeline. Sequence increases only after successful encoding.

## Safety

Deployment events are logical criteria only. `ActuatorController` has no
hardware output API and the interlock always includes
`kInhibitObserveOnlyBuild`. GPIO16/5/6 remain inactive.

## Native regression suite

`pio test -e native` covers:

- flight state and ObserveOnly interlock;
- altitude selection, filtering, stale timeout, and millis wrap;
- attitude initialization, Madgwick update, yaw hold, filtering, and micros wrap;
- telemetry mapping and shared wire encoder;
- battery divider scaling;
- CSV validity and buffer-boundary behavior.

The current suite contains 43 native test cases. The ESP32-S3 production environment must also compile after every module-boundary change.
