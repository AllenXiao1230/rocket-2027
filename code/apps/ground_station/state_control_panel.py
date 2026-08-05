# state_control_panel.py
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QLabel, QComboBox, QPushButton, QHBoxLayout
from PyQt5.QtCore import Qt, pyqtSignal
from state_machine import FlightState
from rocket_ground_station.ui.presentation import (
    flight_state_name,
    normalized_flight_state,
)


class StateControlPanel(QWidget):
    """
    Flight state panel with dropdown and upload button.
    """

    upload_requested = pyqtSignal(FlightState)
    rtc_sync_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)

        layout = QVBoxLayout(self)
        layout.setSpacing(6)

        self.label = QLabel("State: UNKNOWN")
        self.label.setAlignment(Qt.AlignCenter)
        self.label.setStyleSheet("font-size: 18px; font-weight: bold;")

        self.combo = QComboBox()
        self.combo.addItem("UNKNOWN", None)
        for s in FlightState:
            self.combo.addItem(s.name, s)

        self.upload_btn = QPushButton("Upload")
        self.upload_btn.clicked.connect(self._emit_upload)
        self.upload_btn.setEnabled(False)
        self.upload_btn.setToolTip(
            "State uplink is disabled until a versioned command protocol exists."
        )
        self.rtc_btn = QPushButton("Sync RTC")
        self.rtc_btn.clicked.connect(self._emit_rtc_sync)

        row = QHBoxLayout()
        row.addWidget(self.combo)
        row.addWidget(self.upload_btn)
        row.addWidget(self.rtc_btn)

        layout.addWidget(self.label)
        layout.addLayout(row)

    # ---------- Public API ----------

    def update_state(self, state: FlightState | int | None):
        """Display a known state or explicitly clear the panel to UNKNOWN."""

        normalized = normalized_flight_state(state)
        self.label.setText(f"State: {flight_state_name(normalized)}")
        if normalized is None:
            self.combo.setCurrentIndex(0)
            return

        idx = self.combo.findData(normalized)
        if idx >= 0:
            self.combo.setCurrentIndex(idx)

    # ---------- Internal ----------

    def _emit_upload(self):
        state = self.combo.currentData()
        if isinstance(state, FlightState):
            self.upload_requested.emit(state)

    def _emit_rtc_sync(self):
        self.rtc_sync_requested.emit()
