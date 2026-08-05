from pathlib import Path
import time
# gui_main.py
from PyQt5.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QLabel,
    QGroupBox,
    QSplitter,
)
from PyQt5.QtCore import Qt, QTimer

from .qt_serial_worker import SerialWorker
from rocket_ground_station.application import (
    FlightState,
    TelemetryPipeline,
    TelemetrySnapshot,
    coerce_flight_state,
    decode_error,
)
from rocket_ground_station.application.commands import (
    CommandRejected,
    ConnectionMode,
    build_rtc_sync_command,
)
from rocket_ground_station.application.csv_logger import TelemetryCsvLogger

from rocket_attitude import RocketAttitudeView
from battery_panel import BatteryPanel
from plot_panel import PlotPanel
from event_indicator_panel import EventIndicatorPanel
from state_control_panel import StateControlPanel
from serial_panel import SerialPanel
from mission_time import MissionTimePanel
from map_view import MapView
from gui_widgets import TelemetryPanel, SpeedAltitudePanel, LogControlPanel



def _runtime_data_directory() -> Path:
    """Locate the repository runtime-data directory from either app layout."""

    source = Path(__file__).resolve()
    for parent in source.parents:
        if (parent / "AGENTS.md").is_file() and (
            parent / "protocol" / "schema"
        ).is_dir():
            return parent / "data" / "runs"
    return source.parent / "data" / "runs"


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Rocket Ground Station")
        self.resize(1200, 800)

        # ---- Core logic ----
        self._boot_time_seconds = 0.0
        self._current_flight_state = None
        self._connection_mode = ConnectionMode.LORA_GROUND_RECEIVER
        self._logger = None
        self._reset_frame_state()
        self.telemetry = {
            "time": 0.0,
            "lat": None,
            "lon": None,
            "alt": None,
            "gps_alt": None,
            "baro_alt": None,
            "speed": None,
            "gps_speed": None,
            "baro_speed": None,
            "heading": None,
            "sat": None,
            "accx": None,
            "accy": None,
            "accz": None,
            "roll": None,
            "pitch": None,
            "gyro_x": None,
            "gyro_y": None,
            "gyro_z": None,
            "servo_power": None,
            "servo_angle": None,
            "battery": None,
            "status": None,
            "error": None,
            "water": None,
        }

        # ---- Serial worker (created on demand) ----
        self.serial_worker = None

        # ---- UI ----
        self._init_ui()
        self._diag_timer = QTimer(self)
        self._diag_timer.setInterval(250)
        self._diag_timer.timeout.connect(self._update_stream_diagnostics)
        self._diag_timer.start()
        self._update_stream_diagnostics()
        # Versioned v1/v2 decoding lives in the application pipeline.

    # ---------------- UI ----------------

    def _init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)

        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)

        top_layout = QHBoxLayout()
        top_layout.setSpacing(10)

        self.attitude_view = RocketAttitudeView()
        self.battery_panel = BatteryPanel()
        self.plot_panel = PlotPanel()
        self.event_panel = EventIndicatorPanel()
        self.state_panel = StateControlPanel()
        self.serial_panel = SerialPanel(show_connection_mode=True)
        self.time_panel = MissionTimePanel()
        self.map_view = MapView()
        self.telemetry_panel = TelemetryPanel()
        self.speed_alt_panel = SpeedAltitudePanel()
        self.log_panel = LogControlPanel()

        def wrap(title: str, widget: QWidget) -> QGroupBox:
            box = QGroupBox(title)
            box_layout = QVBoxLayout(box)
            box_layout.setContentsMargins(6, 6, 6, 6)
            box_layout.addWidget(widget)
            return box

        # ---- Column 1: Serial (with status) + Map ----
        col1 = QVBoxLayout()
        col1.setSpacing(8)
        col1.addWidget(wrap("序列埠", self.serial_panel))
        col1.addWidget(wrap("地圖", self.map_view))
        col1.setStretch(0, 0)
        col1.setStretch(1, 1)

        # ---- Column 2: Flight state, events, speed+alt, attitude ----
        col2 = QVBoxLayout()
        col2.setSpacing(8)
        fs_row = QHBoxLayout()
        fs_row.setSpacing(6)
        fs_row.addWidget(wrap("飛行狀態控制", self.state_panel))
        fs_row.addWidget(wrap("記錄控制", self.log_panel))
        fs_row_widget = QWidget()
        fs_row_widget.setLayout(fs_row)

        col2.addWidget(fs_row_widget)
        col2.addWidget(wrap("火箭事件指示器", self.event_panel))
        col2.addWidget(wrap("火箭姿態", self.attitude_view))
        col2.setStretch(0, 0)
        col2.setStretch(1, 1)
        col2.setStretch(2, 2)

        # ---- Column 3: Time+Battery inline, other sensor data, speed/alt ----
        col3 = QVBoxLayout()
        col3.setSpacing(8)
        time_batt = QHBoxLayout()
        time_batt.setSpacing(6)
        time_batt.addWidget(self.time_panel)
        time_batt.addWidget(self.battery_panel)
        time_batt_widget = QWidget()
        time_batt_widget.setLayout(time_batt)
        col3.addWidget(wrap("時間 / 電壓", time_batt_widget))
        col3.addWidget(wrap("速度 / 高度", self.speed_alt_panel))
        col3.addWidget(wrap("感測器數據", self.telemetry_panel))
        col3.setStretch(0, 0)
        col3.setStretch(1, 0)
        col3.setStretch(2, 1)

        top_layout.addLayout(col1)
        top_layout.addLayout(col2)
        top_layout.addLayout(col3)

        # Even width for three columns
        top_layout.setStretch(0, 1)
        top_layout.setStretch(1, 1)
        top_layout.setStretch(2, 1)

        top_widget = QWidget()
        top_widget.setLayout(top_layout)

        splitter = QSplitter()
        splitter.setOrientation(Qt.Vertical)
        splitter.addWidget(top_widget)
        splitter.addWidget(wrap("高度曲線", self.plot_panel))
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)

        main_layout.addWidget(splitter)

        # Serial panel callbacks
        self.serial_panel.connect_requested.connect(self._start_serial)
        self.serial_panel.disconnect_requested.connect(self._stop_serial)
        self.state_panel.upload_requested.connect(self._on_state_upload)
        self.state_panel.rtc_sync_requested.connect(self._on_rtc_sync)
        self.log_panel.logging_toggled.connect(self._on_logging_toggled)

    # ---------------- Serial ----------------

    def _start_serial(self, port: str, baudrate: int):
        if self.serial_worker:
            return

        self._reset_frame_state()
        self._connection_mode = self.serial_panel.selected_connection_mode()
        self.serial_worker = SerialWorker(port, baudrate)
        self.serial_worker.data_received.connect(self._on_serial_data)
        self.serial_worker.error_occurred.connect(self._on_serial_status)
        self.serial_worker.status_changed.connect(self._on_serial_status)
        self.serial_worker.start()

    def _stop_serial(self):
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker = None
        self._reset_frame_state()
        self.serial_panel.set_status("Status: DISCONNECTED", color="red")
        self._connection_mode = ConnectionMode.LORA_GROUND_RECEIVER

    def _on_serial_data(self, raw: bytes):
        if raw:
            if self._first_rx_monotonic is None:
                self._first_rx_monotonic = time.monotonic()
            self._rx_bytes += len(raw)
        self._maybe_log_non_telemetry_serial(raw)
        self._process_telemetry_stream(raw)
        self._update_stream_diagnostics()

    def closeEvent(self, event):
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker = None
        self._close_logger()
        event.accept()

    # ---------------- Handlers ----------------

    def _on_attitude(self, roll: float, pitch: float, yaw: float):
        self.telemetry["roll"] = roll
        self.telemetry["pitch"] = pitch
        self.telemetry["heading"] = yaw
        self._refresh_telemetry()

    def _on_battery(self, voltage: float):
        self.battery_panel.update_voltage(voltage)
        self.telemetry["battery"] = voltage
        self._refresh_telemetry()

    def _on_altitude(self, altitude: float):
        self.plot_panel.update_altitude(altitude)
        # Treat incoming altitude as barometric if separate GPS alt arrives later
        self.telemetry["alt"] = altitude
        self.telemetry["baro_alt"] = altitude
        self._refresh_telemetry()

    def _on_event(self, message: str):
        self.event_panel.add_event(message)

    def _on_state_changed(self, state: FlightState | None):
        self.state_panel.update_state(state)

    def _clear_current_state(self):
        """Clear stale state from both state displays when input is unavailable."""

        self._current_flight_state = None
        if hasattr(self, "state_panel"):
            self.state_panel.update_state(None)
        if hasattr(self, "telemetry"):
            self.telemetry["status"] = None
            if hasattr(self, "telemetry_panel"):
                self.telemetry_panel.update_data(self.telemetry)

    def _on_time(self, seconds: float):
        self._boot_time_seconds = seconds
        self.time_panel.update_time(seconds)
        self.telemetry["time"] = self._boot_time_seconds
        self._refresh_telemetry()

    def _on_gps(self, lat: float, lon: float, alt: float):
        self.map_view.update_gps(lat, lon, alt)
        self.telemetry["lat"] = lat
        self.telemetry["lon"] = lon
        self.telemetry["gps_alt"] = alt
        self._refresh_telemetry()

    def _refresh_telemetry(self):
        self.telemetry_panel.update_data(self.telemetry)
        self.speed_alt_panel.update_data(
            self.telemetry.get("gps_speed"),
            self.telemetry.get("baro_speed"),
            self.telemetry.get("gps_alt"),
            self.telemetry.get("baro_alt"),
        )
        self.attitude_view.update_attitude(
            self.telemetry.get("roll", 0.0),
            self.telemetry.get("pitch", 0.0),
            self.telemetry.get("heading", 0.0),
        )

    def _on_serial_status(self, message: str):
        self.event_panel.add_event(message)
        msg_lower = message.lower()
        if "disconnect" in msg_lower or "failed" in msg_lower:
            self._reset_frame_state()
            if "failed" in msg_lower and self.serial_worker is not None:
                failed_worker = self.serial_worker
                failed_worker.stop()
                if self.serial_worker is failed_worker:
                    self.serial_worker = None
            self.serial_panel.set_connected(False)
            self.serial_panel.set_status(message, color="red")
        elif "connected" in msg_lower:
            self.serial_panel.set_connected(True)
            self.serial_panel.set_status(message, color="green")
        else:
            self.serial_panel.set_status(message)

    def _on_state_upload(self, state: FlightState):
        # Placeholder: hook actual upload to rocket when protocol is defined.
        self.event_panel.add_event(f"Upload state requested: {state.name}")

    def _on_rtc_sync(self):
        if not self.serial_worker:
            self.event_panel.add_event("RTC sync failed: serial not connected")
            return
        epoch = int(time.time())
        try:
            cmd = build_rtc_sync_command(self._connection_mode, epoch)
        except CommandRejected as exc:
            self.event_panel.add_event(f"RTC sync blocked: {exc.reason}")
            return
        if not self.serial_worker.write(cmd):
            self.event_panel.add_event("RTC sync failed: serial write failed")
            return
        self.event_panel.add_event(f"RTC sync sent: {epoch}")

    # ---------------- Versioned telemetry stream ----------------

    def _reset_frame_state(self):
        if hasattr(self, "_pipeline"):
            self._pipeline.reset()
        else:
            self._pipeline = TelemetryPipeline()
        self._link_logged = False
        self._saw_hb_text = False
        self._boot_markers_seen = set()
        self._text_serial_logged = False
        self._rx_bytes = 0
        self._first_rx_monotonic = None
        self._last_frame_monotonic = None
        self._non_telemetry_warning_logged = False
        self._last_error_code = None
        self._last_boot_session = None
        self._clear_current_state()
        if hasattr(self, "serial_panel"):
            self.serial_panel.reset_diagnostics()

    def _maybe_log_non_telemetry_serial(self, raw: bytes):
        if not raw or self._link_logged:
            return

        text = raw.decode("ascii", errors="ignore")
        if text:
            for line in text.splitlines():
                marker = line.strip()
                if marker.startswith("BOOT:") and marker not in self._boot_markers_seen:
                    self.event_panel.add_event(f"[FW] {marker}")
                    self._boot_markers_seen.add(marker)

        if b"HB\n" in raw:
            self._saw_hb_text = True

        if self._text_serial_logged:
            return

        printable = sum(1 for b in raw if 32 <= b <= 126 or b in (9, 10, 13))
        if (
            len(raw) >= 4 and printable == len(raw)
            and "BOOT:" not in text
            and "HB" not in text
        ):
            self.event_panel.add_event(
                "收到的是文字序列資料，不是 47/64-byte telemetry frame。"
            )
            self._text_serial_logged = True

    def _process_telemetry_stream(self, data: bytes):
        snapshots = self._pipeline.feed(data)
        if snapshots:
            self._last_frame_monotonic = time.monotonic()
        for snapshot in snapshots:
            self._apply_snapshot(snapshot)

    def _process_legacy_stream(self, data: bytes):
        """Compatibility replay hook; both v1 and v2 now use one pipeline."""

        self._process_telemetry_stream(data)

    def _update_stream_diagnostics(self):
        now = time.monotonic()
        last_frame_age_ms = None
        if self._last_frame_monotonic is not None:
            last_frame_age_ms = int((now - self._last_frame_monotonic) * 1000.0)

        diagnostics = self._pipeline.diagnostics
        self.serial_panel.set_diagnostics(
            rx_bytes=self._rx_bytes,
            valid_frames=diagnostics.total_frames,
            crc_fail=diagnostics.parser_crc_failures,
            resync_count=diagnostics.parser_resync_count,
            last_frame_age_ms=last_frame_age_ms,
            v1_frames=diagnostics.v1_frames,
            v2_frames=diagnostics.v2_frames,
            sequence_gaps=diagnostics.sequence_gaps,
            sequence_anomalies=diagnostics.sequence_anomalies,
        )

        if (
            self._rx_bytes > 0
            and diagnostics.total_frames == 0
            and self._first_rx_monotonic is not None
            and (now - self._first_rx_monotonic) >= 2.0
        ):
            if self._saw_hb_text:
                msg = "收到 HB/BOOT 回應，但尚未收到 v1/v2 telemetry（尚未進入 loop 或 port/baud 不符）"
            else:
                msg = "收到資料但非有效 47/64-byte telemetry（port/baud 不符）"
            self.serial_panel.set_diagnostic_notice(msg, color="#a35f00")
            if not self._non_telemetry_warning_logged:
                self.event_panel.add_event(msg)
                self._non_telemetry_warning_logged = True
            return

        if diagnostics.total_frames > 0:
            self.serial_panel.set_diagnostic_notice("v1/v2 telemetry 正常接收中。", color="green")
        elif self._rx_bytes > 0:
            self.serial_panel.set_diagnostic_notice("已收到資料，正在等待有效 telemetry frame...", color="#666")
        else:
            self.serial_panel.set_diagnostic_notice("")

    def _apply_snapshot(self, snapshot: TelemetrySnapshot):
        if not self._link_logged:
            self.event_panel.add_event(
                f"偵測到 telemetry v{snapshot.protocol_version} frame。"
            )
            self._link_logged = True

        if (
            snapshot.boot_session is not None
            and snapshot.boot_session != self._last_boot_session
        ):
            if self._last_boot_session is not None:
                self.event_panel.add_event(
                    f"Flight computer reboot/session change: "
                    f"boot_id={snapshot.boot_id}"
                )
            self._last_boot_session = snapshot.boot_session

        self._boot_time_seconds = snapshot.uptime_s
        state = coerce_flight_state(snapshot.flight_state)
        if state is None:
            self._clear_current_state()
        elif state != self._current_flight_state:
            self._current_flight_state = state
            self._on_state_changed(state)

        if (
            snapshot.error_code is not None
            and snapshot.error_code != self._last_error_code
        ):
            if snapshot.error_code:
                self.event_panel.add_event(
                    f"Flight error: {decode_error(snapshot.error_code)}"
                )
            elif self._last_error_code not in (None, 0):
                self.event_panel.add_event("Flight errors cleared")
            self._last_error_code = snapshot.error_code

        self.time_panel.update_time(snapshot.mission_time_s)
        if snapshot.primary_altitude_m is not None:
            self.plot_panel.update_altitude(snapshot.primary_altitude_m)
        if snapshot.gps_position is not None:
            latitude, longitude = snapshot.gps_position
            self.map_view.update_gps(
                latitude,
                longitude,
                snapshot.gps_altitude_m,
            )
        self.battery_panel.update_voltage(snapshot.battery_v)

        # Text widgets receive every optional value.  They render None as
        # unavailable rather than retaining or fabricating a zero.
        self.telemetry.update({
            "time": snapshot.uptime_s,
            "lat": snapshot.latitude_deg,
            "lon": snapshot.longitude_deg,
            "alt": snapshot.primary_altitude_m,
            "gps_alt": snapshot.gps_altitude_m,
            "baro_alt": snapshot.barometric_altitude_m,
            "speed": snapshot.gps_speed_mps,
            "gps_speed": snapshot.gps_speed_mps,
            "baro_speed": None,
            "heading": snapshot.heading_deg,
            "accx": snapshot.acceleration_x_g,
            "accy": snapshot.acceleration_y_g,
            "accz": snapshot.acceleration_z_g,
            "roll": snapshot.roll_deg,
            "pitch": snapshot.pitch_deg,
            "gyro_x": snapshot.gyro_x_dps,
            "gyro_y": snapshot.gyro_y_dps,
            "gyro_z": snapshot.gyro_z_dps,
            "servo_power": snapshot.servo_power_v,
            "servo_angle": snapshot.servo_angle_deg,
            "sat": snapshot.satellite_count,
            "battery": snapshot.battery_v,
            "status": snapshot.flight_state,
            "error": snapshot.error_code,
            "water": snapshot.water_detected,
        })
        self._refresh_telemetry()

        if self._logger:
            try:
                self._logger.log(snapshot, self._pipeline.diagnostics)
                self._logger.flush()
            except Exception as exc:
                self.event_panel.add_event(f"CSV write failed: {exc}")
                self._close_logger()
                self.log_panel.set_active(False)

    # ---------------- Logging ----------------

    def _on_logging_toggled(self, enable: bool):
        if enable:
            try:
                self._logger = TelemetryCsvLogger(
                    _runtime_data_directory()
                )
                self.event_panel.add_event(
                    f"開始記錄 CSV: {self._logger.path}"
                )
                self.log_panel.set_active(True)
            except Exception as e:
                self.event_panel.add_event(f"記錄啟動失敗: {e}")
                self.log_panel.set_active(False)
        else:
            self._close_logger()
            self.log_panel.set_active(False)
            self.event_panel.add_event("停止記錄 CSV")

    def _close_logger(self):
        logger = self._logger
        self._logger = None
        if logger is None:
            return
        try:
            logger.close()
        except Exception as exc:
            self.event_panel.add_event(f"CSV close failed: {exc}")
