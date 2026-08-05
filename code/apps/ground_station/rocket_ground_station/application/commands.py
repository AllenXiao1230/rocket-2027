"""Outbound command policy for supported physical connection modes."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class ConnectionMode(str, Enum):
    """How the ground-station computer is physically connected."""

    DIRECT_FLIGHT_COMPUTER_UART = "direct_flight_computer_uart"
    LORA_GROUND_RECEIVER = "lora_ground_receiver"


@dataclass(frozen=True)
class CommandPolicy:
    mode: ConnectionMode
    uplink_allowed: bool
    rtc_sync_allowed: bool
    rejection_reason: str = ""


class CommandRejected(PermissionError):
    """Raised when a command is unsafe or unsupported for the active link."""

    def __init__(
        self,
        mode: ConnectionMode,
        command_name: str,
        reason: str,
    ) -> None:
        self.mode = mode
        self.command_name = command_name
        self.reason = reason
        super().__init__(
            f"{command_name} rejected for {mode.value}: {reason}"
        )


_POLICIES = {
    ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART: CommandPolicy(
        mode=ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART,
        uplink_allowed=True,
        rtc_sync_allowed=True,
    ),
    ConnectionMode.LORA_GROUND_RECEIVER: CommandPolicy(
        mode=ConnectionMode.LORA_GROUND_RECEIVER,
        uplink_allowed=False,
        rtc_sync_allowed=False,
        rejection_reason=(
            "the production LoRa ground receiver is a downlink-only binary "
            "passthrough; connect directly to the flight-computer UART"
        ),
    ),
}


def policy_for(mode: ConnectionMode | str) -> CommandPolicy:
    """Return the immutable policy for a connection mode."""

    return _POLICIES[ConnectionMode(mode)]


def authorize_uplink(
    mode: ConnectionMode | str,
    payload: bytes | bytearray | memoryview,
    *,
    command_name: str = "uplink",
) -> bytes:
    """Validate a raw uplink against the selected physical-link policy."""

    policy = policy_for(mode)
    if not policy.uplink_allowed:
        raise CommandRejected(
            policy.mode,
            command_name,
            policy.rejection_reason,
        )
    if not isinstance(payload, (bytes, bytearray, memoryview)):
        raise TypeError("command payload must be bytes-like")
    encoded = bytes(payload)
    if not encoded:
        raise ValueError("command payload cannot be empty")
    return encoded


def build_rtc_sync_command(
    mode: ConnectionMode | str,
    epoch: int,
) -> bytes:
    """Build the existing ``RTC_SYNC`` command only for direct UART."""

    policy = policy_for(mode)
    if not policy.rtc_sync_allowed:
        raise CommandRejected(
            policy.mode,
            "RTC sync",
            policy.rejection_reason,
        )
    if isinstance(epoch, bool) or not isinstance(epoch, int):
        raise TypeError("epoch must be an integer Unix timestamp")
    if epoch <= 0 or epoch > 0xFFFFFFFF:
        raise ValueError("epoch must fit the flight firmware uint32 range")
    return authorize_uplink(
        policy.mode,
        f"RTC_SYNC:{epoch}\n".encode("ascii"),
        command_name="RTC sync",
    )
