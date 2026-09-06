# Telemetry v2

Telemetry v2 is the production binary format shared by the flight computer,
ground receiver and ground station.

## Envelope

- Sync: `A5 5A`
- Version: `2`
- Length: `64`
- Message type: `1` (telemetry)
- Endianness: little
- Integrity: CRC-16/CCITT-FALSE over bytes 0–61; CRC is stored little-endian
- Identity: `(boot_id, sequence)`

`sequence` increases once for every frame produced. The same encoded bytes are
sent over LoRa and UART. It wraps modulo 2^32. `boot_id` changes on reboot.

## Validity

`valid_flags` is authoritative. Consumers must not infer validity from a numeric
zero. Invalid signed scalar fields use their minimum value, invalid `u16`
scalar fields use `0xFFFF`, and invalid `u8` scalar fields use `0xFF`.

`error_code` is the exception: it is an 8-bit bitmask, so `0xFF` is the valid
"all eight faults active" value whenever `VALID_ERROR_CODE` is set. When that
flag is clear the wire default is also `0xFF`, but consumers must report it as
missing.

| Bit | Meaning |
| ---: | --- |
| 0 | GPS position |
| 1 | GPS altitude |
| 2 | GPS speed |
| 3 | Satellite count |
| 4 | Roll/pitch attitude |
| 5 | Heading |
| 6 | Gyroscope |
| 7 | Acceleration |
| 8 | Barometric altitude |
| 9 | Battery |
| 10 | Servo power |
| 11 | Servo angle |
| 12 | Flight state |
| 13 | Error code |
| 14 | Water input |

`source_flags` bit 0 means heading from IMU, bit 1 heading from GPS, bit 2 state
altitude from barometer, and bit 3 state altitude from GPS.

The exact offsets and machine-readable contract are in
`schema/telemetry_v2.json`.

## Deployment rule

Flight firmware sends v2 only over both LoRa and UART. Ground Receiver and
Ground Station reject every non-v2 frame. Never transmit more than one format
in the same flight cycle.
