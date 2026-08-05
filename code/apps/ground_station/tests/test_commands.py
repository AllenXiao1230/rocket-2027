import unittest

from rocket_ground_station.application.commands import (
    CommandRejected,
    ConnectionMode,
    authorize_uplink,
    build_rtc_sync_command,
    policy_for,
)


class CommandPolicyTests(unittest.TestCase):
    def test_direct_uart_allows_existing_rtc_sync_wire_command(self):
        command = build_rtc_sync_command(
            ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART,
            1_800_000_000,
        )

        self.assertEqual(command, b"RTC_SYNC:1800000000\n")
        self.assertTrue(
            policy_for(
                ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART
            ).rtc_sync_allowed
        )

    def test_ground_receiver_rejects_rtc_sync_and_all_uplink(self):
        with self.assertRaises(CommandRejected) as context:
            build_rtc_sync_command(
                ConnectionMode.LORA_GROUND_RECEIVER,
                1_800_000_000,
            )
        self.assertIn("downlink-only", str(context.exception))

        with self.assertRaises(CommandRejected):
            authorize_uplink(
                ConnectionMode.LORA_GROUND_RECEIVER,
                b"ANY_COMMAND\n",
            )

    def test_direct_uart_authorizes_binary_payload_without_mutation(self):
        payload = b"\x00\xffcommand"
        self.assertEqual(
            authorize_uplink(
                ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART,
                payload,
            ),
            payload,
        )

    def test_rtc_epoch_validation_matches_firmware_uint32(self):
        direct = ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART
        with self.assertRaises(TypeError):
            build_rtc_sync_command(direct, True)
        with self.assertRaises(TypeError):
            build_rtc_sync_command(direct, 1.5)
        with self.assertRaises(ValueError):
            build_rtc_sync_command(direct, 0)
        with self.assertRaises(ValueError):
            build_rtc_sync_command(direct, 0x1_0000_0000)


if __name__ == "__main__":
    unittest.main()
