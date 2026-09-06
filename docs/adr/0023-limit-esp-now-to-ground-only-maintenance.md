# Limit ESP-NOW to ground-only maintenance

ESP-NOW is a Ground-Only Maintenance Link: it may be used in `SAFE`, `READY` and Ground Test Session, but must be disabled before `ARMED` and remain disabled throughout flight and recovery. This preserves the frequency-diverse Backup RF candidate from a same-band 2.4 GHz common fault and keeps all Recovery Authority, recovery and ARM functions on their independent approved paths.

## Considered Options

Keeping ESP-NOW active in flight with a 2.4 GHz Backup RF would require a radio arbiter, time-slot proof and final-vehicle coexistence evidence, yet would still not provide frequency independence. Moving the Backup RF to a different band remains possible if the 2027 Competition Acceptance Gate requires it.
