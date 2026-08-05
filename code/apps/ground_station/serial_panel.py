"""Qt serial controls backed by the UI-independent transport discovery API."""

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from rocket_ground_station.application.commands import ConnectionMode
from rocket_ground_station.transport.serial_ports import (
    discover_serial_ports,
    port_label,
)


class SerialPanel(QWidget):
    """Select a physical link and emit connect/disconnect requests."""

    connect_requested = pyqtSignal(str, int)
    disconnect_requested = pyqtSignal()

    def __init__(self, parent=None, *, show_connection_mode: bool = False):
        super().__init__(parent)

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(4)

        controls = QHBoxLayout()
        controls.addWidget(QLabel("Port:"))
        self.port_box = QComboBox()
        controls.addWidget(self.port_box)

        self.mode_label = QLabel("Link:")
        controls.addWidget(self.mode_label)
        self.mode_box = QComboBox()
        self.mode_box.addItem(
            "LoRa receiver / unknown (downlink only)",
            ConnectionMode.LORA_GROUND_RECEIVER,
        )
        self.mode_box.addItem(
            "Direct flight UART (uplink enabled)",
            ConnectionMode.DIRECT_FLIGHT_COMPUTER_UART,
        )
        controls.addWidget(self.mode_box)
        self.mode_label.setVisible(show_connection_mode)
        self.mode_box.setVisible(show_connection_mode)

        controls.addWidget(QLabel("Baud:"))
        self.baud_box = QComboBox()
        self.baud_box.addItems(
            [
                "9600",
                "38400",
                "57600",
                "115200",
                "230400",
                "460800",
                "921600",
            ]
        )
        self.baud_box.setCurrentText("115200")
        controls.addWidget(self.baud_box)

        self.refresh_btn = QPushButton("Refresh")
        self.connect_btn = QPushButton("Connect")
        self.disconnect_btn = QPushButton("Disconnect")
        self.disconnect_btn.setEnabled(False)
        controls.addWidget(self.refresh_btn)
        controls.addWidget(self.connect_btn)
        controls.addWidget(self.disconnect_btn)

        self.refresh_btn.clicked.connect(self._refresh_ports)
        self.connect_btn.clicked.connect(self._on_connect)
        self.disconnect_btn.clicked.connect(self._on_disconnect)
        outer.addLayout(controls)

        self.status_label = QLabel("Status: DISCONNECTED")
        self.status_label.setStyleSheet("color: red; font-weight: bold;")
        outer.addWidget(self.status_label)

        self.diag_label = QLabel("")
        self.diag_label.setStyleSheet("color: #666;")
        outer.addWidget(self.diag_label)

        self.notice_label = QLabel("")
        self.notice_label.setWordWrap(True)
        self.notice_label.setStyleSheet(
            "color: #a35f00; font-weight: bold;"
        )
        outer.addWidget(self.notice_label)

        self._refresh_ports()
        self.reset_diagnostics()

    def _refresh_ports(self):
        selected_device = self.port_box.currentData()
        self.port_box.clear()
        try:
            ports = discover_serial_ports()
        except RuntimeError as exc:
            self.port_box.addItem(str(exc), "")
            self.connect_btn.setEnabled(False)
            return

        if not ports:
            self.port_box.addItem("No serial ports detected", "")
            self.connect_btn.setEnabled(False)
            return

        for port in ports:
            self.port_box.addItem(port_label(port), port.device)

        restored_index = -1
        if selected_device:
            restored_index = self.port_box.findData(selected_device)
        self.port_box.setCurrentIndex(
            restored_index if restored_index >= 0 else 0
        )
        self.connect_btn.setEnabled(True)

    def _on_connect(self):
        self._refresh_ports()
        port = self.port_box.currentData()
        if not port:
            return
        baud = int(self.baud_box.currentText())
        self.connect_requested.emit(port, baud)
        self.connect_btn.setEnabled(False)
        self.disconnect_btn.setEnabled(True)
        self.mode_box.setEnabled(False)
        self.set_status("Status: CONNECTING...", color="orange")

    def _on_disconnect(self):
        self.disconnect_requested.emit()
        self.set_connected(False)
        self.set_status("Status: DISCONNECTED", color="red")
        self.reset_diagnostics()

    def selected_connection_mode(self) -> ConnectionMode:
        mode = self.mode_box.currentData()
        if isinstance(mode, ConnectionMode):
            return mode
        return ConnectionMode.LORA_GROUND_RECEIVER

    def set_connected(self, connected: bool):
        has_port = bool(self.port_box.currentData())
        self.connect_btn.setEnabled(not connected and has_port)
        self.refresh_btn.setEnabled(not connected)
        self.disconnect_btn.setEnabled(connected)
        self.mode_box.setEnabled(not connected)

    def set_status(self, text: str, color: str | None = None):
        self.status_label.setText(text)
        if color:
            self.status_label.setStyleSheet(
                f"color: {color}; font-weight: bold;"
            )

    def set_diagnostics(
        self,
        rx_bytes: int,
        valid_frames: int,
        crc_fail: int,
        resync_count: int,
        last_frame_age_ms: int | None,
        *,
        v1_frames: int = 0,
        v2_frames: int = 0,
        sequence_gaps: int = 0,
        sequence_anomalies: int = 0,
    ):
        age_text = (
            "--"
            if last_frame_age_ms is None
            else f"{int(last_frame_age_ms)} ms"
        )
        self.diag_label.setText(
            "Diag: "
            f"rx_bytes={rx_bytes} "
            f"valid_frames={valid_frames} "
            f"v1={v1_frames} "
            f"v2={v2_frames} "
            f"crc_fail={crc_fail} "
            f"resync={resync_count} "
            f"seq_gaps={sequence_gaps} "
            f"seq_anomalies={sequence_anomalies} "
            f"last_frame_age={age_text}"
        )

    def set_diagnostic_notice(
        self,
        text: str = "",
        color: str = "#a35f00",
    ):
        self.notice_label.setText(text)
        self.notice_label.setStyleSheet(
            f"color: {color}; font-weight: bold;"
        )

    def reset_diagnostics(self):
        self.set_diagnostics(
            rx_bytes=0,
            valid_frames=0,
            crc_fail=0,
            resync_count=0,
            last_frame_age_ms=None,
        )
        self.set_diagnostic_notice("")
