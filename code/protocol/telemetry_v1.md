# Telemetry v1 — frozen compatibility format

The original production packet is retained only for receiver and parser
compatibility.

- Sync: `55 AA`
- Length: 47 bytes
- Endianness: little
- Integrity: XOR of bytes 0–45 stored at byte 46
- No version, length, sequence, boot identifier or general validity flags

The historical field table is in the legacy flight-computer README. New code
must not extend or transmit v1; use telemetry v2 instead.
