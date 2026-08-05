import serial

try:
    from . import _app_path  # noqa: F401
except ImportError:
    import _app_path  # type: ignore[no-redef]  # noqa: F401

from rocket_ground_station.application import TelemetryPipeline, decode_error


PORT = "/dev/tty.usbserial-56BC0010791"
BAUD = 115200


def main():
    pipeline = TelemetryPipeline()
    with serial.Serial(PORT, BAUD, timeout=0.2) as ser:
        while True:
            raw = ser.read(ser.in_waiting or 1)
            for packet in pipeline.feed(raw):
                state_text = (
                    "---" if packet.flight_state is None
                    else str(packet.flight_state)
                )
                baro_text = (
                    "---" if packet.barometric_altitude_m is None
                    else f"{packet.barometric_altitude_m:7.1f}m"
                )
                gps_text = (
                    "---" if packet.gps_altitude_m is None
                    else f"{packet.gps_altitude_m:7.1f}m"
                )
                battery_text = (
                    "---" if packet.battery_v is None
                    else f"{packet.battery_v:5.2f}V"
                )
                print(
                    f"T+{packet.mission_time_s:6.1f}s "
                    f"state={state_text} "
                    f"baro={baro_text} "
                    f"gps={gps_text} "
                    f"vbat={battery_text} "
                    f"err={decode_error(packet.error_code)}"
                )


if __name__ == "__main__":
    main()
