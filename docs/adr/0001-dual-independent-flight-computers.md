# Use two independent flight-computer fault domains

Rocket Avionics will use a Main and Backup ESP32-S3 flight computer. They share only the power-MUX-backed GNSS source; each keeps independent sensing, state estimation, RF, storage, power and recovery paths. The firmware will share pure algorithm and protocol cores while using separate deployable targets and hardware adapters, so a common codebase does not create a shared runtime or recovery-output fault domain.
