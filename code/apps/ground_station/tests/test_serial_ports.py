import unittest
from types import SimpleNamespace

from rocket_ground_station.transport.serial_ports import (
    SerialPort,
    discover_serial_ports,
    port_label,
    select_serial_port,
)


class SerialPortDiscoveryTests(unittest.TestCase):
    def test_discovery_normalizes_deduplicates_and_prioritizes_usb_uart(self):
        infos = [
            SimpleNamespace(
                device="/dev/cu.Bluetooth-Incoming-Port",
                description="Bluetooth",
                hwid="",
            ),
            SimpleNamespace(
                device="/dev/cu.usbmodem2101",
                description="ESP32-S3 USB JTAG/serial debug unit",
                hwid="USB VID:PID=303A:1001",
                vid=0x303A,
                pid=0x1001,
                serial_number="ABC",
            ),
            SimpleNamespace(
                device="/dev/cu.usbmodem2101",
                description="duplicate",
                hwid="",
            ),
            SimpleNamespace(device="", description="invalid", hwid=""),
            SimpleNamespace(
                device="/dev/ttyS0",
                description="Built-in UART",
                hwid="",
            ),
        ]

        ports = discover_serial_ports(lambda: infos)

        self.assertEqual(
            [port.device for port in ports],
            [
                "/dev/cu.usbmodem2101",
                "/dev/ttyS0",
                "/dev/cu.Bluetooth-Incoming-Port",
            ],
        )
        self.assertEqual(ports[0].vid, 0x303A)
        self.assertEqual(ports[0].serial_number, "ABC")

    def test_select_restores_exact_device_before_priority_fallback(self):
        ports = [
            SerialPort("/dev/cu.usbserial-A", "USB UART"),
            SerialPort("/dev/ttyS0", "Built-in"),
        ]

        restored = select_serial_port(ports, "/dev/ttyS0")
        fallback = select_serial_port(ports, "/dev/missing")

        self.assertEqual(restored.device, "/dev/ttyS0")
        self.assertEqual(fallback.device, "/dev/cu.usbserial-A")
        self.assertIsNone(select_serial_port([]))

    def test_port_label_omits_empty_or_na_description(self):
        self.assertEqual(port_label(SerialPort("/dev/a")), "/dev/a")
        self.assertEqual(
            port_label(SerialPort("/dev/a", "N/A")),
            "/dev/a",
        )
        self.assertEqual(
            port_label(SerialPort("/dev/a", "Receiver")),
            "/dev/a (Receiver)",
        )


if __name__ == "__main__":
    unittest.main()
