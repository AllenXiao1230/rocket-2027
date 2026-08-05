#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <SD.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>

// 引入感測器函式庫
#include "Adafruit_SHT31.h"
#include "Adafruit_BMP3XX.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_ADXL375.h"
#include "ICM42688.h"
#include "board/pin_map.h"
#include "drivers/battery_monitor.h"
#include "drivers/lora_transmitter.h"
#include "error_codes.h"
#include "flight/actuator_controller.h"
#include "flight/flight_state_machine.h"
#include "flight/hardware_interlock.h"
#include <TelemetryProtocol.h>
#include "navigation/altitude_estimator.h"
#include "navigation/attitude_estimator.h"

#include "model/telemetry_sample.h"
#include "telemetry/telemetry_builder.h"
#include "model/telemetry_storage_record.h"
#include "storage/sd_telemetry_store.h"
#ifndef ARDUINO_USB_MODE
#define ARDUINO_USB_MODE 0
#endif
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT != 0
#error "USB CDC must be disabled. Use UART0 (COM) for telemetry."
#endif

// On ESP32-S3 with ARDUINO_USB_CDC_ON_BOOT=0, Serial maps to UART0 (COM).
#define GS_SERIAL_PORT Serial

// Set to 1 for debug logs over UART0. Keep 0 when UART0 is used for binary packets.
#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
#define DBG_PRINTF(...) GS_SERIAL_PORT.printf(__VA_ARGS__)
#define DBG_PRINTLN(msg) GS_SERIAL_PORT.println(msg)
#else
#define DBG_PRINTF(...) do {} while (0)
#define DBG_PRINTLN(msg) do {} while (0)
#endif

#define NOTE_DO 262
#define NOTE_RE 294
#define NOTE_MI 330
#define NOTE_SOL 392
#define NOTE_HDO 523

// --- 建立感測器物件 ---
Adafruit_SHT31 sht31(&Wire1);
Adafruit_BMP3XX bmp;
Adafruit_ADXL375 accel_a(12345, &Wire);
Adafruit_ADXL375 accel_b(12345, &Wire1);
Adafruit_ADXL375 *accel = &accel_a;
ICM42688 imu_a_68(Wire, 0x68);   // IMU42688 地址通常是 0x68 或 0x69
ICM42688 imu_a_69(Wire, 0x69);
ICM42688 imu_b_68(Wire1, 0x68);
ICM42688 imu_b_69(Wire1, 0x69);
ICM42688 *imu = &imu_a_68;
uint8_t imu_addr = 0x68;
const char *imu_bus_label = "I2C_A";
const char *adxl_bus_label = "I2C_A";
RTC_DS3231 rtc;
Adafruit_NeoPixel status_led(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);
HardwareSerial GPSSerial(1);
HardwareSerial BackupSerial(2);
rocket::drivers::BatteryMonitor battery_monitor(
    BATTERY_ADC_PIN, 200000.0f, 100000.0f);
SPIClass loraSPI(HSPI);
rocket::drivers::LoraTransmitterConfig lora_config = {
    {
        LORA_SCK,
        LORA_MISO,
        LORA_MOSI,
        LORA_NSS,
        LORA_RST,
        LORA_DIO0,
        LORA_DIO1,
    },
    {
        LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_PREAMBLE_LEN,
        LORA_SYNC_WORD, LORA_TX_POWER, PA_OUTPUT_PA_BOOST_PIN,
        true, false,
    },
};
rocket::drivers::LoraTransmitter lora_transmitter(
    loraSPI, LoRa, lora_config);
#if defined(VSPI)
SPIClass sdSPI(VSPI);
#else
SPIClass sdSPI(FSPI);
#endif
File sd_log;
bool lora_ready = false;
rocket::storage::SdTelemetryStore sd_telemetry_store;
bool sd_ready = false;

// --- 狀態旗標 (檢查誰活著) ---
bool status_sht = false;
bool status_bmp = false;
bool status_adxl = false;
bool status_imu = false;
bool rtc_ready = false;

// 與地面站一致的飛行狀態定義
static const uint8_t kStateTest = 0;
static const uint8_t kStateIdle = 1;
static const uint8_t kStatePreflight = 2;
static const uint8_t kStateAscent = 3;
static const uint8_t kStateApogee = 4;
static const uint8_t kStateDescent = 5;
static const uint8_t kStateLanded = 6;
static const uint8_t kStateAbort = 99;

// --- Versioned telemetry is defined in protocol/schema/telemetry_v2.json. ---
static const float kG = 9.80665f;
// Set to 0 to disable low-battery error.
static const uint16_t kBatteryLowMv = 0;

static const uint8_t kRtcSyncLineLen = 64;
static const uint32_t kGroundStationBaud = 115200;
static const bool kEnableBmp390 = false;
static const bool kEnableStartupTone = false;
static const uint32_t kI2cClockHz = 100000;
static const uint32_t kI2cClockFallbackHz1 = 50000;
static const uint32_t kI2cClockFallbackHz2 = 25000;
static const uint16_t kI2cTimeoutMs = 20;
static const uint8_t kAdxlAltAddress = 0x1D;
// 0 = keep retrying forever until sensors respond.
static const uint32_t kSensorRetryWindowMs = 0;
static const uint32_t kSensorRetryIntervalMs = 1000;
static const bool kEnableBackupHeartbeat = false;
static const bool kEnableSetupAsciiDiagnostics = true;
static const uint32_t kDeferredInitStepGapMs = 5;
static const uint32_t kGpsSentenceTimeoutMs = 3000;
static const uint32_t kGpsBaudRotateIntervalMs = 4000;
static const uint16_t kGpsCourseMinSpeedDms = 20; // 2.0 m/s
static const uint32_t kGpsBaudCandidates[] = {
  UART_GPS_BAUD, 115200, 57600, 38400, 9600
};
static const uint8_t kGpsBaudCandidateCount = static_cast<uint8_t>(
  sizeof(kGpsBaudCandidates) / sizeof(kGpsBaudCandidates[0])
);
static const uint32_t kLoraRetryIntervalMs = 2000;
static const uint8_t kLoraTxFailReinitThreshold = 3;

enum BootInitStage : uint8_t {
  kBootInitI2c = 0,
  kBootInitLoRa,
  kBootInitSd,
  kBootInitRtc,
  kBootInitEnv,
  kBootInitMotion,
  kBootInitDone,
};

static int32_t gps_lat_raw = 0;
static int32_t gps_lon_raw = 0;
static int16_t gps_alt_dm = 0;
static int16_t gps_speed_dms = 0;
static uint16_t gps_course_ddeg = 0;
static uint8_t gps_sat_count = 0;
static uint32_t gps_last_fix_ms = 0;
static uint32_t gps_last_altitude_ms = 0;
static uint32_t gps_last_speed_ms = 0;
static uint32_t gps_last_satellite_ms = 0;
static uint32_t gps_last_sentence_ms = 0;
static uint32_t gps_last_course_ms = 0;
static uint32_t gps_last_baud_switch_ms = 0;
static uint8_t gps_baud_idx = 0;
static const uint32_t kGpsTimeoutMs = 2000;
static bool gps_fix_valid = false;
static bool gps_altitude_valid = false;
static bool gps_speed_valid = false;
static bool gps_satellite_valid = false;
static bool gps_course_valid = false;

static uint8_t flight_state = kStateIdle;
static rocket::flight::FlightStateMachine flight_state_machine;
static rocket::navigation::AltitudeEstimator altitude_estimator;
static rocket::navigation::AttitudeEstimator attitude_estimator;
static rocket::flight::ActuatorController actuator_controller;
static rocket::flight::ActuatorStatus actuator_status;
static bool launch_committed = false;
static uint32_t last_heartbeat_ms = 0;
static HardwareSerial &ground_station_serial = GS_SERIAL_PORT;
static uint32_t sensor_init_window_start_ms = 0;
static uint32_t last_sensor_retry_ms = 0;
static uint32_t i2c_a_runtime_clock_hz = kI2cClockHz;
static uint32_t i2c_b_runtime_clock_hz = kI2cClockHz;
static BootInitStage boot_init_stage = kBootInitI2c;
static uint32_t last_deferred_init_ms = 0;
static uint32_t last_lora_retry_ms = 0;
static uint8_t lora_tx_fail_count = 0;
static uint16_t telemetry_boot_id = 0;
static uint32_t telemetry_sequence = 0;

static inline int16_t clamp_i16(long v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

static inline uint16_t clamp_u16(long v) {
  if (v < 0) return 0;
  if (v > 65535) return 65535;
  return static_cast<uint16_t>(v);
}

static float parse_nmea_coord(const char *field) {
  if (!field || !*field) {
    return NAN;
  }
  double val = atof(field);
  int deg = static_cast<int>(val / 100.0);
  double minutes = val - (deg * 100.0);
  double dec = deg + minutes / 60.0;
  return static_cast<float>(dec);
}

static bool parse_nmea_course_ddeg(const char *field, uint16_t &course_ddeg) {
  if (!field || !*field) {
    return false;
  }
  float course_deg = static_cast<float>(atof(field));
  if (isnan(course_deg)) {
    return false;
  }
  long course = lroundf(course_deg * 10.0f);
  while (course < 0) {
    course += 3600;
  }
  while (course >= 3600) {
    course -= 3600;
  }
  course_ddeg = static_cast<uint16_t>(course);
  return true;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool nmea_checksum_ok(const char *line) {
  if (!line || line[0] != '$') {
    return false;
  }
  const char *star = strchr(line, '*');
  if (!star || star[1] == '\0' || star[2] == '\0') {
    return false;
  }
  uint8_t sum = 0;
  for (const char *p = line + 1; p < star; ++p) {
    sum ^= static_cast<uint8_t>(*p);
  }
  int hi = hex_nibble(star[1]);
  int lo = hex_nibble(star[2]);
  if (hi < 0 || lo < 0) {
    return false;
  }
  uint8_t expected = static_cast<uint8_t>((hi << 4) | lo);
  return sum == expected;
}

static void handle_gps_line(char *line) {
  if (!line || line[0] != '$') {
    return;
  }
  if (!nmea_checksum_ok(line)) {
    return;
  }
  const uint32_t now_ms = millis();
  gps_last_sentence_ms = now_ms;

  char *save = nullptr;
  char *type = strtok_r(line, ",", &save);
  if (!type) {
    return;
  }
  bool is_rmc = (strstr(type, "RMC") != nullptr);
  bool is_gga = (strstr(type, "GGA") != nullptr);
  if (!is_rmc && !is_gga) {
    return;
  }

  if (is_rmc) {
    // RMC: time, status, lat, N/S, lon, E/W, speed(knots), ...
    char *time_f = strtok_r(nullptr, ",", &save);
    char *status_f = strtok_r(nullptr, ",", &save);
    char *lat_f = strtok_r(nullptr, ",", &save);
    char *ns_f = strtok_r(nullptr, ",", &save);
    char *lon_f = strtok_r(nullptr, ",", &save);
    char *ew_f = strtok_r(nullptr, ",", &save);
    char *speed_f = strtok_r(nullptr, ",", &save);
    char *course_f = strtok_r(nullptr, ",", &save);
    (void)time_f;
    if (status_f && status_f[0] == 'A') {
      bool got_position = false;
      float lat = parse_nmea_coord(lat_f);
      float lon = parse_nmea_coord(lon_f);
      if (!isnan(lat) && ns_f && ns_f[0] == 'S') {
        lat = -lat;
      }
      if (!isnan(lon) && ew_f && ew_f[0] == 'W') {
        lon = -lon;
      }
      if (!isnan(lat) && !isnan(lon)) {
        gps_lat_raw = static_cast<int32_t>(lroundf(lat * 1e7f));
        gps_lon_raw = static_cast<int32_t>(lroundf(lon * 1e7f));
        got_position = true;
      }
      if (speed_f && *speed_f) {
        float speed_kn = static_cast<float>(atof(speed_f));
        float speed_ms = speed_kn * 0.514444f;
        gps_speed_dms = clamp_i16(lroundf(speed_ms * 10.0f));
        gps_speed_valid = true;
        gps_last_speed_ms = now_ms;
      }
      uint16_t course_ddeg = 0;
      if (parse_nmea_course_ddeg(course_f, course_ddeg)) {
        gps_course_ddeg = course_ddeg;
        gps_course_valid = true;
        gps_last_course_ms = now_ms;
      }
      if (got_position) {
        gps_last_fix_ms = now_ms;
        gps_fix_valid = true;
      }
    } else {
      gps_fix_valid = false;
      gps_speed_valid = false;
      gps_speed_dms = 0;
    }
    return;
  }

  if (is_gga) {
    // GGA: time, lat, N/S, lon, E/W, fix, sats, hdop, alt(m), ...
    char *time_f = strtok_r(nullptr, ",", &save);
    char *lat_f = strtok_r(nullptr, ",", &save);
    char *ns_f = strtok_r(nullptr, ",", &save);
    char *lon_f = strtok_r(nullptr, ",", &save);
    char *ew_f = strtok_r(nullptr, ",", &save);
    char *fix_f = strtok_r(nullptr, ",", &save);
    char *sat_f = strtok_r(nullptr, ",", &save);
    char *hdop_f = strtok_r(nullptr, ",", &save);
    char *alt_f = strtok_r(nullptr, ",", &save);
    (void)time_f;
    (void)hdop_f;
    if (sat_f && *sat_f) {
      int sat_raw = atoi(sat_f);
      if (sat_raw < 0) {
        sat_raw = 0;
      } else if (sat_raw > 255) {
        sat_raw = 255;
      }
      gps_sat_count = static_cast<uint8_t>(sat_raw);
      gps_satellite_valid = true;
      gps_last_satellite_ms = now_ms;
    }
    int fix_quality = (fix_f && *fix_f) ? atoi(fix_f) : 0;
    if (fix_quality > 0) {
      bool got_position = false;
      float lat = parse_nmea_coord(lat_f);
      float lon = parse_nmea_coord(lon_f);
      if (!isnan(lat) && ns_f && ns_f[0] == 'S') {
        lat = -lat;
      }
      if (!isnan(lon) && ew_f && ew_f[0] == 'W') {
        lon = -lon;
      }
      if (!isnan(lat) && !isnan(lon)) {
        gps_lat_raw = static_cast<int32_t>(lroundf(lat * 1e7f));
        gps_lon_raw = static_cast<int32_t>(lroundf(lon * 1e7f));
        got_position = true;
      }
      if (alt_f && *alt_f) {
        float alt_m = static_cast<float>(atof(alt_f));
        gps_alt_dm = clamp_i16(lroundf(alt_m * 10.0f));
        gps_altitude_valid = true;
        gps_last_altitude_ms = now_ms;
      }
      if (got_position) {
        gps_last_fix_ms = now_ms;
        gps_fix_valid = true;
      }
    } else {
      gps_fix_valid = false;
      gps_altitude_valid = false;
    }
  }
}

static void process_gps_stream() {
  static char line[256];
  static uint16_t idx = 0;
  static bool dropping_line = false;
  while (GPSSerial.available()) {
    char c = static_cast<char>(GPSSerial.read());
    if (c == '\n') {
      if (!dropping_line) {
        line[idx] = '\0';
      }
      if (!dropping_line && idx > 0) {
        handle_gps_line(line);
      }
      idx = 0;
      dropping_line = false;
      continue;
    }
    if (c == '\r') {
      continue;
    }
    if (dropping_line) {
      continue;
    }
    if (idx < static_cast<uint16_t>(sizeof(line) - 1)) {
      line[idx++] = c;
    } else {
      // Drop oversized sentence and wait for next newline to re-sync.
      dropping_line = true;
      idx = 0;
    }
  }
}

static void set_gps_uart_baud(uint32_t baud) {
  GPSSerial.end();
  delay(2);
  GPSSerial.begin(baud, SERIAL_8N1, UART_GPS_RX, UART_GPS_TX);
}

static void maybe_rotate_gps_baud(uint32_t now_ms) {
  bool sentence_recent = false;
  if (gps_last_sentence_ms > 0) {
    sentence_recent = (now_ms - gps_last_sentence_ms) < kGpsBaudRotateIntervalMs;
  }
  if (sentence_recent) {
    return;
  }
  if ((now_ms - gps_last_baud_switch_ms) < kGpsBaudRotateIntervalMs) {
    return;
  }
  gps_baud_idx = static_cast<uint8_t>((gps_baud_idx + 1) % kGpsBaudCandidateCount);
  uint32_t next_baud = kGpsBaudCandidates[gps_baud_idx];
  set_gps_uart_baud(next_baud);
  gps_last_baud_switch_ms = now_ms;
  gps_last_sentence_ms = now_ms;
  gps_fix_valid = false;
  gps_altitude_valid = false;
  gps_speed_valid = false;
  gps_satellite_valid = false;
  gps_course_valid = false;
  gps_sat_count = 0;
  gps_speed_dms = 0;

  if (kEnableSetupAsciiDiagnostics) {
    ground_station_serial.print("BOOT:GPS_BAUD=");
    ground_station_serial.println(next_baud);
  }
}

static void handle_rtc_sync_stream(Stream &port, char *line, uint8_t &idx) {
  while (port.available()) {
    char c = static_cast<char>(port.read());
    if (c == '\n' || c == '\r') {
      if (idx == 0) {
        continue;
      }
      line[idx] = '\0';
      if (strncmp(line, "RTC_SYNC:", 9) == 0) {
        uint32_t epoch = static_cast<uint32_t>(strtoul(line + 9, nullptr, 10));
        if (epoch > 0 && rtc_ready) {
          rtc.adjust(DateTime(epoch));
          DBG_PRINTF("RTC synced: %lu\n", static_cast<unsigned long>(epoch));
        }
      }
      idx = 0;
    } else {
      if (idx < kRtcSyncLineLen - 1) {
        line[idx++] = c;
      }
    }
  }
}

static void handle_serial_rtc_sync() {
  static char gs_line[kRtcSyncLineLen];
  static uint8_t gs_idx = 0;
  handle_rtc_sync_stream(ground_station_serial, gs_line, gs_idx);
}

static void emit_setup_diag(const char *tag) {
  if (!kEnableSetupAsciiDiagnostics || !tag) {
    return;
  }
  ground_station_serial.print("BOOT:");
  ground_station_serial.println(tag);
}

static void emit_setup_diag_u32(const char *tag, uint32_t value) {
  if (!kEnableSetupAsciiDiagnostics || !tag) {
    return;
  }
  ground_station_serial.print("BOOT:");
  ground_station_serial.print(tag);
  ground_station_serial.print("=");
  ground_station_serial.println(value);
}

static bool i2c_probe(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

static uint8_t i2c_count_devices(TwoWire &bus) {
#if DEBUG_SERIAL
  uint8_t count = 0;
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    if (i2c_probe(bus, addr)) {
      count++;
    }
  }
  return count;
#else
  // Fast-path for boot speed: probe only known sensor addresses when debug is off.
  static const uint8_t known_addrs[] = {0x44, 0x76, 0x77, 0x53, kAdxlAltAddress, 0x68, 0x69};
  uint8_t count = 0;
  for (size_t i = 0; i < sizeof(known_addrs); i++) {
    if (i2c_probe(bus, known_addrs[i])) {
      count++;
    }
  }
  return count;
#endif
}

static void i2c_scan_print(TwoWire &bus, const char *label);

static void i2c_bus_recover(int sda_pin, int scl_pin) {
  pinMode(sda_pin, INPUT_PULLUP);
  pinMode(scl_pin, INPUT_PULLUP);
  delay(1);
  if (digitalRead(sda_pin) == HIGH && digitalRead(scl_pin) == HIGH) {
    return;
  }

  pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(scl_pin, HIGH);
  for (uint8_t i = 0; i < 18 && digitalRead(sda_pin) == LOW; i++) {
    digitalWrite(scl_pin, LOW);
    delayMicroseconds(5);
    digitalWrite(scl_pin, HIGH);
    delayMicroseconds(5);
  }

  pinMode(sda_pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda_pin, LOW);
  delayMicroseconds(5);
  digitalWrite(scl_pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda_pin, HIGH);
  delayMicroseconds(5);

  pinMode(sda_pin, INPUT_PULLUP);
  pinMode(scl_pin, INPUT_PULLUP);
}

static uint8_t init_i2c_bus(TwoWire &bus, int sda_pin, int scl_pin, const char *label) {
  static const uint32_t kI2cClockCandidates[] = {
    kI2cClockHz, kI2cClockFallbackHz1, kI2cClockFallbackHz2
  };
  uint8_t count = 0;
  bool recovered = false;
  uint32_t selected_clock = kI2cClockHz;

  for (uint8_t pass = 0; pass < 2 && count == 0; pass++) {
    if (pass == 1) {
      i2c_bus_recover(sda_pin, scl_pin);
      recovered = true;
    }
    for (size_t i = 0; i < sizeof(kI2cClockCandidates) / sizeof(kI2cClockCandidates[0]); i++) {
      bus.end();
      delay(2);
      bus.setPins(sda_pin, scl_pin);
      bus.begin(sda_pin, scl_pin);
      bus.setClock(kI2cClockCandidates[i]);
      bus.setTimeOut(kI2cTimeoutMs);
      count = i2c_count_devices(bus);
      if (count > 0) {
        selected_clock = kI2cClockCandidates[i];
        break;
      }
    }
  }

  if (count == 0) {
    // Keep bus initialized at primary clock even when no device was found.
    bus.end();
    delay(2);
    bus.setPins(sda_pin, scl_pin);
    bus.begin(sda_pin, scl_pin);
    bus.setClock(kI2cClockHz);
    bus.setTimeOut(kI2cTimeoutMs);
    selected_clock = kI2cClockHz;
  } else if (kEnableSetupAsciiDiagnostics) {
    char tag[24];
    snprintf(tag, sizeof(tag), "%s_CLK", label ? label : "I2C");
    emit_setup_diag_u32(tag, selected_clock);
    if (recovered) {
      snprintf(tag, sizeof(tag), "%s_RECOVER", label ? label : "I2C");
      emit_setup_diag(tag);
    }
  }

  if (sda_pin == I2C_A_SDA && scl_pin == I2C_A_SCL) {
    i2c_a_runtime_clock_hz = selected_clock;
  } else if (sda_pin == I2C_B_SDA && scl_pin == I2C_B_SCL) {
    i2c_b_runtime_clock_hz = selected_clock;
  }

  i2c_scan_print(bus, label);
  return count;
}

static void i2c_scan_print(TwoWire &bus, const char *label) {
#if !DEBUG_SERIAL
  (void)bus;
  (void)label;
  return;
#else
  DBG_PRINTF("%s scan:\n", label);
  bool found_any = false;
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    if (i2c_probe(bus, addr)) {
      DBG_PRINTF("  - 0x%02X\n", addr);
      found_any = true;
    }
  }
  if (!found_any) {
    DBG_PRINTLN("  (no devices)");
  }
#endif
}

static bool init_adxl_sensor() {
  uint8_t adxl_addr = ADXL375_ADDRESS;
  accel = &accel_a;
  adxl_bus_label = "I2C_A";

  if (i2c_probe(Wire, ADXL375_ADDRESS)) {
    accel = &accel_a;
    adxl_bus_label = "I2C_A";
    adxl_addr = ADXL375_ADDRESS;
  } else if (i2c_probe(Wire, kAdxlAltAddress)) {
    accel = &accel_a;
    adxl_bus_label = "I2C_A";
    adxl_addr = kAdxlAltAddress;
  } else if (i2c_probe(Wire1, ADXL375_ADDRESS)) {
    accel = &accel_b;
    adxl_bus_label = "I2C_B";
    adxl_addr = ADXL375_ADDRESS;
  } else if (i2c_probe(Wire1, kAdxlAltAddress)) {
    accel = &accel_b;
    adxl_bus_label = "I2C_B";
    adxl_addr = kAdxlAltAddress;
  }

  status_adxl = accel->begin(adxl_addr);
  if (status_adxl) {
    DBG_PRINTF("ADXL375 OK on %s\n", adxl_bus_label);
  } else {
    DBG_PRINTLN("ADXL375 init failed on both I2C_A/I2C_B");
  }
  return status_adxl;
}

static bool init_imu_sensor() {
  bool imu_present = false;
  if (i2c_probe(Wire, 0x68)) {
    imu = &imu_a_68;
    imu_addr = 0x68;
    imu_bus_label = "I2C_A";
    imu_present = true;
  } else if (i2c_probe(Wire, 0x69)) {
    imu = &imu_a_69;
    imu_addr = 0x69;
    imu_bus_label = "I2C_A";
    imu_present = true;
  } else if (i2c_probe(Wire1, 0x68)) {
    imu = &imu_b_68;
    imu_addr = 0x68;
    imu_bus_label = "I2C_B";
    imu_present = true;
  } else if (i2c_probe(Wire1, 0x69)) {
    imu = &imu_b_69;
    imu_addr = 0x69;
    imu_bus_label = "I2C_B";
    imu_present = true;
  }

  if (!imu_present) {
    status_imu = false;
    DBG_PRINTLN("ICM42688 not detected on 0x68/0x69");
    return false;
  }

  int imu_status = imu->begin();
  if (imu_status < 0) {
    status_imu = false;
    DBG_PRINTF("ICM42688 init failed on %s @0x%02X\n", imu_bus_label, imu_addr);
  } else {
    status_imu = true;
    DBG_PRINTF("ICM42688 OK on %s @0x%02X\n", imu_bus_label, imu_addr);
    // 設定 IMU 範圍 (火箭通常需要最大範圍)
    imu->setAccelFS(ICM42688::gpm16); // ±16g
    imu->setGyroFS(ICM42688::dps2000); // ±2000 dps
  }

  // ICM42688 library sets I2C clock to 400kHz internally.
  // Force both buses back to our stable system-wide clock.
  Wire.setClock(i2c_a_runtime_clock_hz);
  Wire1.setClock(i2c_b_runtime_clock_hz);
  return status_imu;
}

static void retry_delayed_sensor_init(uint32_t now_ms) {
  if (status_adxl && status_imu) {
    return;
  }
  if (sensor_init_window_start_ms == 0) {
    return;
  }
  if (kSensorRetryWindowMs > 0 &&
      now_ms - sensor_init_window_start_ms > kSensorRetryWindowMs) {
    return;
  }
  if (now_ms - last_sensor_retry_ms < kSensorRetryIntervalMs) {
    return;
  }
  last_sensor_retry_ms = now_ms;

  init_i2c_bus(Wire, I2C_A_SDA, I2C_A_SCL, "I2C_A");
  init_i2c_bus(Wire1, I2C_B_SDA, I2C_B_SCL, "I2C_B");

  if (!status_adxl) {
    init_adxl_sensor();
  }
  if (!status_imu) {
    init_imu_sensor();
  }
}

static void init_lora_subsystem() {
  lora_ready = lora_transmitter.begin();
  if (lora_ready) {
    lora_tx_fail_count = 0;
    DBG_PRINTLN("LoRa OK");
  } else {
    DBG_PRINTLN("LoRa init failed");
  }
  emit_setup_diag(lora_ready ? "LORA_OK" : "LORA_FAIL");
}

static bool lora_send_frame(const uint8_t *frame, size_t len) {
  if (!lora_ready || !frame || len == 0) {
    return false;
  }
  return lora_transmitter.send(
             frame,
             len,
             rocket::drivers::LoraPostTransmitMode::kStandby) ==
         rocket::drivers::LoraTransmitResult::kSent;
}

static void maintain_lora_link(uint32_t now_ms) {
  if (lora_ready) {
    return;
  }
  if (boot_init_stage != kBootInitDone) {
    return;
  }
  if ((now_ms - last_lora_retry_ms) < kLoraRetryIntervalMs) {
    return;
  }
  last_lora_retry_ms = now_ms;
  init_lora_subsystem();
  if (lora_ready) {
    emit_setup_diag("LORA_RECOVERED");
  }
}

static void init_sd_subsystem() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  sd_ready = SD.begin(SD_CS, sdSPI);
  if (sd_ready) {
    sd_log = SD.open("/telemetry.csv", FILE_APPEND);
    if (!sd_log ||
        !sd_telemetry_store.attach(sd_log) ||
        !sd_telemetry_store.writeHeaderIfEmpty()) {
      sd_ready = false;
    }
  }
  emit_setup_diag(sd_ready ? "SD_OK" : "SD_FAIL");
}

static void init_rtc_subsystem() {
  rtc_ready = rtc.begin(&Wire1);
  if (rtc_ready) {
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
  }
  emit_setup_diag(rtc_ready ? "RTC_OK" : "RTC_FAIL");
}

static void init_env_subsystem() {
  status_sht = sht31.begin(0x44);

  status_bmp = false;
  if (kEnableBmp390) {
    status_bmp = bmp.begin_I2C();
    if (status_bmp) {
      bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
      bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
      bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
      bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    }
  }
  emit_setup_diag(status_sht ? "SHT_OK" : "SHT_FAIL");
}

static void init_motion_subsystem(uint32_t now_ms) {
  init_adxl_sensor();
  init_imu_sensor();
  sensor_init_window_start_ms = now_ms;
  last_sensor_retry_ms = now_ms;
  emit_setup_diag((status_adxl && status_imu) ? "IMU_ADXL_OK" : "IMU_ADXL_RETRY");
}

static void run_deferred_boot_init(uint32_t now_ms) {
  if (boot_init_stage == kBootInitDone) {
    return;
  }
  if ((now_ms - last_deferred_init_ms) < kDeferredInitStepGapMs) {
    return;
  }
  last_deferred_init_ms = now_ms;

  switch (boot_init_stage) {
    case kBootInitI2c: {
      uint8_t i2c_a_count = init_i2c_bus(Wire, I2C_A_SDA, I2C_A_SCL, "I2C_A");
      uint8_t i2c_b_count = init_i2c_bus(Wire1, I2C_B_SDA, I2C_B_SCL, "I2C_B");
      emit_setup_diag_u32("I2C_A_DEV", i2c_a_count);
      emit_setup_diag_u32("I2C_B_DEV", i2c_b_count);
      emit_setup_diag((i2c_a_count + i2c_b_count) > 0 ? "I2C_READY" : "I2C_NO_DEV");
      boot_init_stage = kBootInitLoRa;
      break;
    }
    case kBootInitLoRa:
      init_lora_subsystem();
      boot_init_stage = kBootInitSd;
      break;
    case kBootInitSd:
      init_sd_subsystem();
      boot_init_stage = kBootInitRtc;
      break;
    case kBootInitRtc:
      init_rtc_subsystem();
      boot_init_stage = kBootInitEnv;
      break;
    case kBootInitEnv:
      init_env_subsystem();
      boot_init_stage = kBootInitMotion;
      break;
    case kBootInitMotion:
      init_motion_subsystem(now_ms);
      boot_init_stage = kBootInitDone;
      emit_setup_diag("BOOT_INIT_DONE");
      break;
    case kBootInitDone:
    default:
      break;
  }
}

static void buzzerTone(int freq, int duration_ms) {
  if (freq <= 0) {
    ledcWriteTone(BUZZER_CH, 0);
    delay(duration_ms);
    return;
  }
  ledcWriteTone(BUZZER_CH, freq);
  delay(duration_ms);
  ledcWriteTone(BUZZER_CH, 0);
}

static void set_status_led(uint8_t state) {
  uint32_t color = status_led.Color(0, 0, 0);
  switch (state) {
    case kStateTest:
      color = status_led.Color(255, 255, 255);
      break;
    case kStateIdle:
      color = status_led.Color(0, 0, 255);
      break;
    case kStatePreflight:
      color = status_led.Color(0, 255, 255);
      break;
    case kStateAscent:
      color = status_led.Color(0, 255, 0);
      break;
    case kStateApogee:
      color = status_led.Color(255, 200, 0);
      break;
    case kStateDescent:
      color = status_led.Color(255, 80, 0);
      break;
    case kStateLanded:
      color = status_led.Color(120, 60, 0);
      break;
    case kStateAbort:
      color = status_led.Color(255, 0, 0);
      break;
    default:
      color = status_led.Color(32, 32, 32);
      break;
  }
  status_led.setPixelColor(0, color);
  status_led.show();
}

void setup() {
  ground_station_serial.begin(kGroundStationBaud);
  delay(1);
  emit_setup_diag("SETUP_ENTER");
  ground_station_serial.print("HB\n");

  pinMode(SAFETY_SWITCH_PIN, INPUT_PULLUP);
  pinMode(SEPARATION_SERVO_PIN, OUTPUT);
  digitalWrite(SEPARATION_SERVO_PIN, LOW);
  pinMode(CAMERA_A_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAMERA_A_TRIGGER_PIN, LOW);
  pinMode(CAMERA_B_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAMERA_B_TRIGGER_PIN, LOW);
  actuator_controller.reset();

  status_led.begin();
  status_led.setBrightness(32);
  set_status_led(kStateIdle);

  pinMode(WATER_DETECT_PIN, INPUT);
  pinMode(DS3231_SQW_PIN, INPUT_PULLUP);

  ledcSetup(BUZZER_CH, 2000, BUZZER_RES);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);

  battery_monitor.begin();

  gps_baud_idx = 0;
  gps_last_sentence_ms = 0;
  gps_last_fix_ms = 0;
  gps_last_altitude_ms = 0;
  gps_last_speed_ms = 0;
  gps_last_satellite_ms = 0;
  gps_last_course_ms = 0;
  gps_fix_valid = false;
  gps_altitude_valid = false;
  gps_speed_valid = false;
  gps_satellite_valid = false;
  gps_course_valid = false;
  gps_last_baud_switch_ms = millis();
  set_gps_uart_baud(kGpsBaudCandidates[gps_baud_idx]);
  emit_setup_diag_u32("GPS_BAUD", kGpsBaudCandidates[gps_baud_idx]);
  pinMode(UART_GPS_PPS, INPUT);

  if (kEnableBackupHeartbeat) {
    BackupSerial.begin(UART_SUB_BAUD, SERIAL_8N1, UART_SUB_RX, UART_SUB_TX);
  }

  boot_init_stage = kBootInitI2c;
  last_deferred_init_ms = millis();
  sensor_init_window_start_ms = 0;
  last_sensor_retry_ms = 0;
  last_lora_retry_ms = 0;
  lora_tx_fail_count = 0;
  telemetry_boot_id = static_cast<uint16_t>(esp_random() & 0xFFFF);
  if (telemetry_boot_id == 0) {
    telemetry_boot_id = 1;
  }
  telemetry_sequence = 0;

  emit_setup_diag("SETUP_DONE");
  ground_station_serial.print("HB\n");
}

void loop() {
  uint32_t time_ms = millis();
  static bool loop_enter_reported = false;
  if (!loop_enter_reported) {
    emit_setup_diag("LOOP_ENTER");
    loop_enter_reported = true;
  }
  run_deferred_boot_init(time_ms);
  maintain_lora_link(time_ms);
  if (kEnableBackupHeartbeat && time_ms - last_heartbeat_ms >= 100) {
    BackupSerial.write('H');
    BackupSerial.write('B');
    BackupSerial.write('\n');
    last_heartbeat_ms = time_ms;
  }
  handle_serial_rtc_sync();
  process_gps_stream();
  if (gps_last_fix_ms > 0 && time_ms - gps_last_fix_ms > kGpsTimeoutMs) {
    gps_fix_valid = false;
  }
  if (gps_last_altitude_ms > 0 &&
      time_ms - gps_last_altitude_ms > kGpsTimeoutMs) {
    gps_altitude_valid = false;
  }
  if (gps_last_speed_ms > 0 &&
      time_ms - gps_last_speed_ms > kGpsTimeoutMs) {
    gps_speed_valid = false;
    gps_speed_dms = 0;
  }
  if (gps_last_satellite_ms > 0 &&
      time_ms - gps_last_satellite_ms > kGpsTimeoutMs) {
    gps_satellite_valid = false;
  }
  if (gps_last_course_ms > 0 && time_ms - gps_last_course_ms > kGpsSentenceTimeoutMs) {
    gps_course_valid = false;
  }
  maybe_rotate_gps_baud(time_ms);
  retry_delayed_sensor_init(time_ms);

  if (kEnableStartupTone && boot_init_stage == kBootInitDone) {
    static bool startup_tone_done = false;
    if (!startup_tone_done) {
      // Keep tone out of setup so loop can start immediately.
      delay(1000);
      buzzerTone(NOTE_DO, 120);
      delay(20);
      buzzerTone(NOTE_MI, 120);
      delay(20);
      buzzerTone(NOTE_SOL, 120);
      delay(20);
      buzzerTone(NOTE_HDO, 120);
      startup_tone_done = true;
    }
  }

  float temp_c = 0.0f;
  float hum = 0.0f;
  if (status_sht) {
    temp_c = sht31.readTemperature();
    hum = sht31.readHumidity();
  }

  float pressure_pa = 0.0f;
  float baro_alt_m = 0.0f;
  bool baro_valid = false;
  if (kEnableBmp390 && status_bmp && bmp.performReading()) {
    pressure_pa = bmp.pressure;
    baro_alt_m = bmp.readAltitude(1013.25);
    baro_valid = true;
  }

  float accx_g = 0.0f;
  float accy_g = 0.0f;
  float accz_g = 0.0f;
  if (status_adxl) {
    sensors_event_t event;
    accel->getEvent(&event);
    float adxl_x = event.acceleration.x / kG;
    float adxl_y = event.acceleration.y / kG;
    float adxl_z = event.acceleration.z / kG;
    // Rocket axes: X=right, Y=forward (launch), Z=up/nose.
    accx_g = -adxl_y;  // Rocket X = -ADXL Y
    accy_g = adxl_x;   // Rocket Y = ADXL X
    accz_g = adxl_z;   // Rocket Z = ADXL Z
  }

  float roll_deg = 0.0f;
  float pitch_deg = 0.0f;
  uint16_t heading_ddeg = 0;
  bool heading_from_imu = false;
  float gyro_x_dps = 0.0f;
  float gyro_y_dps = 0.0f;
  float gyro_z_dps = 0.0f;
  static uint32_t last_imu_print_ms = 0;
  rocket::navigation::AttitudeInputs attitude_inputs;
  attitude_inputs.now_us = micros();
  attitude_inputs.imu_valid = status_imu;
  if (status_imu) {
    imu->getAGT();
    const float gx_raw = imu->gyrX();
    const float gy_raw = imu->gyrY();
    const float gz_raw = imu->gyrZ();
    const float ax_raw = imu->accX();
    const float ay_raw = imu->accY();
    const float az_raw = imu->accZ();
    gyro_x_dps = gx_raw;
    gyro_y_dps = gy_raw;
    gyro_z_dps = gz_raw;

    if (time_ms - last_imu_print_ms >= 500) {
      DBG_PRINTF("ICM acc[g]=%.3f,%.3f,%.3f gyro[dps]=%.3f,%.3f,%.3f\n",
                 ax_raw, ay_raw, az_raw, gx_raw, gy_raw, gz_raw);
      last_imu_print_ms = time_ms;
    }

    // Keep the hardware-specific axis mapping outside the pure estimator.
    // Fusion axes preserve the legacy mapping: roll=Z, pitch=X, yaw=Y.
    attitude_inputs.body_accel_x_g = az_raw;
    attitude_inputs.body_accel_y_g = ax_raw;
    attitude_inputs.body_accel_z_g = ay_raw;
    attitude_inputs.body_gyro_x_dps = gz_raw;
    attitude_inputs.body_gyro_y_dps = gx_raw;
    attitude_inputs.body_gyro_z_dps = gy_raw;
  }

  const rocket::navigation::AttitudeEstimate attitude =
      attitude_estimator.update(attitude_inputs);
  roll_deg = attitude.roll_deg;
  pitch_deg = attitude.pitch_deg;
  heading_from_imu = attitude.heading_valid;
  if (heading_from_imu) {
    heading_ddeg = clamp_u16(lroundf(attitude.heading_deg * 10.0f));
  }

  if (!heading_from_imu &&
      gps_speed_valid &&
      gps_course_valid &&
      gps_speed_dms >= static_cast<int16_t>(kGpsCourseMinSpeedDms)) {
    heading_ddeg = gps_course_ddeg;
  }

  float accel_g = sqrtf(accx_g * accx_g + accy_g * accy_g + accz_g * accz_g);

  rocket::navigation::AltitudeInputs altitude_inputs;
  altitude_inputs.now_ms = time_ms;
  altitude_inputs.barometer_valid = baro_valid;
  altitude_inputs.barometer_altitude_m = baro_alt_m;
  altitude_inputs.gps_valid = gps_altitude_valid;
  altitude_inputs.gps_altitude_m = gps_alt_dm / 10.0f;
  const rocket::navigation::AltitudeEstimate navigation_altitude =
      altitude_estimator.update(altitude_inputs);
  const float alt_m = navigation_altitude.altitude_m;
  const bool alt_valid = navigation_altitude.altitude_valid;
  const float vert_speed_ms = navigation_altitude.vertical_speed_mps;
  const bool vert_speed_valid =
      navigation_altitude.vertical_speed_valid;

  uint8_t water_detected = digitalRead(WATER_DETECT_PIN) ? 1 : 0;
  bool safety_armed = (digitalRead(SAFETY_SWITCH_PIN) == HIGH);
  rocket::flight::FlightInputs flight_inputs;
  flight_inputs.now_ms = time_ms;
  // GPIO21 remains a legacy classification input only. INPUT_PULLUP/HIGH can
  // never satisfy the independent physical-actuation interlock.
  flight_inputs.legacy_safety_high = safety_armed;
  flight_inputs.acceleration_valid = status_adxl;
  flight_inputs.acceleration_magnitude_g = accel_g;
  flight_inputs.altitude_valid = alt_valid;
  flight_inputs.altitude_m = alt_m;
  flight_inputs.vertical_speed_valid = vert_speed_valid;
  flight_inputs.vertical_speed_mps = vert_speed_ms;
  flight_inputs.water_detected = water_detected == 1;

  const rocket::flight::FlightEvents flight_events =
      flight_state_machine.update(flight_inputs);
  flight_state = flight_state_machine.stateCode();

  if (flight_events.liftoff_detected) {
    launch_committed = true;
  }
  if (flight_state == kStateIdle || flight_state == kStateAbort) {
    launch_committed = false;
    actuator_controller.reset();
  }

  rocket::flight::DeploymentCriteria deployment_criteria;
  deployment_criteria.drogue =
      flight_events.drogue_criteria_satisfied;
  deployment_criteria.main =
      flight_events.main_criteria_satisfied;
  rocket::flight::InterlockInputs interlock_inputs;
  // Rocket 2027 is deliberately observe-only until dedicated arm,
  // continuity and energy-bus feedback circuits are defined and bench tested.
  interlock_inputs.software_armed = false;
  interlock_inputs.dedicated_arm_circuit_asserted = false;
  interlock_inputs.continuity_confirmed = false;
  interlock_inputs.energy_bus_ready = false;
  interlock_inputs.launch_committed = launch_committed;
  interlock_inputs.fatal_fault = false;
  actuator_status =
      actuator_controller.update(deployment_criteria, interlock_inputs);

  static uint8_t last_state = 0xFF;
  if (flight_state != last_state) {
    set_status_led(flight_state);
    last_state = flight_state;
  }
  uint32_t rtc_unix = 0;
  if (rtc_ready) {
    rtc_unix = rtc.now().unixtime();
  }
  uint16_t battery_mv = battery_monitor.readMillivolts();
  uint8_t error_code = 0;
  if (!lora_ready) {
    error_code |= kErrLoRaInit;
  }
  if (!sd_ready) {
    error_code |= kErrSdInit;
  }
  if (!rtc_ready) {
    error_code |= kErrRtcInit;
  }
  if (kEnableBmp390 && !status_bmp) {
    error_code |= kErrBaroInit;
  }
  if (!status_imu) {
    error_code |= kErrImuInit;
  }
  if (!status_adxl) {
    error_code |= kErrAdxlInit;
  }
  if (flight_state >= kStatePreflight && !gps_fix_valid) {
    error_code |= kErrGpsNoFix;
  }
  if (battery_mv == 0 || (kBatteryLowMv > 0 && battery_mv < kBatteryLowMv)) {
    error_code |= kErrBattery;
  }

  rocket::model::TelemetrySample telemetry_sample;
  telemetry_sample.context.boot_id = telemetry_boot_id;
  telemetry_sample.context.sequence = telemetry_sequence;
  telemetry_sample.context.uptime_ms = time_ms;
  telemetry_sample.gps.position_valid = gps_fix_valid;
  telemetry_sample.gps.latitude_deg =
      static_cast<double>(gps_lat_raw) / 1.0e7;
  telemetry_sample.gps.longitude_deg =
      static_cast<double>(gps_lon_raw) / 1.0e7;
  telemetry_sample.gps.altitude_valid = gps_altitude_valid;
  telemetry_sample.gps.altitude_m = gps_alt_dm / 10.0f;
  telemetry_sample.gps.speed_valid = gps_speed_valid;
  telemetry_sample.gps.speed_mps = gps_speed_dms / 10.0f;
  telemetry_sample.gps.satellite_count_valid = gps_satellite_valid;
  telemetry_sample.gps.satellite_count = gps_sat_count;

  telemetry_sample.navigation.attitude_valid = attitude.attitude_valid;
  telemetry_sample.navigation.roll_deg = roll_deg;
  telemetry_sample.navigation.pitch_deg = pitch_deg;
  const bool heading_from_gps =
      !heading_from_imu && gps_speed_valid && gps_course_valid &&
      gps_speed_dms >= static_cast<int16_t>(kGpsCourseMinSpeedDms);
  telemetry_sample.navigation.heading_valid =
      heading_from_imu || heading_from_gps;
  telemetry_sample.navigation.heading_deg = heading_ddeg / 10.0f;
  telemetry_sample.navigation.heading_source =
      heading_from_imu
          ? rocket::model::HeadingTelemetrySource::kImu
          : (heading_from_gps
                 ? rocket::model::HeadingTelemetrySource::kGps
                 : rocket::model::HeadingTelemetrySource::kNone);
  telemetry_sample.navigation.state_altitude_valid =
      navigation_altitude.altitude_valid;
  if (navigation_altitude.altitude_source ==
      rocket::navigation::AltitudeSource::kBarometer) {
    telemetry_sample.navigation.state_altitude_source =
        rocket::model::StateAltitudeTelemetrySource::kBarometer;
  } else if (navigation_altitude.altitude_source ==
             rocket::navigation::AltitudeSource::kGps) {
    telemetry_sample.navigation.state_altitude_source =
        rocket::model::StateAltitudeTelemetrySource::kGps;
  }

  telemetry_sample.motion.gyro_valid = status_imu;
  telemetry_sample.motion.gyro_x_dps = gyro_x_dps;
  telemetry_sample.motion.gyro_y_dps = gyro_y_dps;
  telemetry_sample.motion.gyro_z_dps = gyro_z_dps;
  telemetry_sample.motion.acceleration_valid = status_adxl;
  telemetry_sample.motion.acceleration_x_g = accx_g;
  telemetry_sample.motion.acceleration_y_g = accy_g;
  telemetry_sample.motion.acceleration_z_g = accz_g;
  telemetry_sample.environment.barometric_altitude_valid = baro_valid;
  telemetry_sample.environment.barometric_altitude_m = baro_alt_m;
  telemetry_sample.power.battery_valid = battery_mv != 0;
  telemetry_sample.power.battery_v = battery_mv / 1000.0f;
  // Servo power/angle remain invalid until real feedback sensors exist.
  telemetry_sample.flight.state_valid = true;
  telemetry_sample.flight.flight_state = flight_state;
  telemetry_sample.flight.error_code_valid = true;
  telemetry_sample.flight.error_code = error_code;
  telemetry_sample.flight.water_valid = true;
  telemetry_sample.flight.water_detected = water_detected != 0;

  const rocket::protocol::TelemetryV2Fields telemetry_fields =
      rocket::telemetry::TelemetryBuilder().build(telemetry_sample);

  uint8_t frame[rocket::protocol::kTelemetryV2FrameLength];
  const size_t frame_length = sizeof(frame);
  if (!rocket::protocol::encodeTelemetryV2(
          telemetry_fields, frame, frame_length)) {
    emit_setup_diag("TELEMETRY_ENCODE_FAIL");
    return;
  }
  telemetry_sequence++;

  if (lora_ready) {
    if (lora_send_frame(frame, frame_length)) {
      lora_tx_fail_count = 0;
    } else {
      if (lora_tx_fail_count < 255) {
        lora_tx_fail_count++;
      }
      if (lora_tx_fail_count >= kLoraTxFailReinitThreshold) {
        lora_ready = false;
        last_lora_retry_ms = time_ms;
        emit_setup_diag("LORA_TX_FAIL");
      }
    }
  }

  if (sd_ready && sd_log) {
    rocket::model::TelemetryStorageRecord record = {};
    record.time_ms = time_ms;
    record.rtc_unix.value = rtc_unix;
    record.rtc_unix.valid = rtc_ready;
    record.latitude_deg.value =
        static_cast<double>(gps_lat_raw) / 1.0e7;
    record.latitude_deg.valid = gps_fix_valid;
    record.longitude_deg.value =
        static_cast<double>(gps_lon_raw) / 1.0e7;
    record.longitude_deg.valid = gps_fix_valid;
    record.gps_altitude_m.value = gps_alt_dm / 10.0f;
    record.gps_altitude_m.valid = gps_altitude_valid;
    record.baro_altitude_m.value = baro_alt_m;
    record.baro_altitude_m.valid = baro_valid;
    record.speed_mps.value = gps_speed_dms / 10.0f;
    record.speed_mps.valid = gps_speed_valid;
    record.heading_deg.value = heading_ddeg / 10.0f;
    record.heading_deg.valid =
        telemetry_sample.navigation.heading_valid;
    record.satellite_count.value = gps_sat_count;
    record.satellite_count.valid = gps_satellite_valid;
    record.roll_deg.value = roll_deg;
    record.roll_deg.valid = attitude.attitude_valid;
    record.pitch_deg.value = pitch_deg;
    record.pitch_deg.valid = attitude.attitude_valid;
    record.accel_x_g.value = accx_g;
    record.accel_x_g.valid = status_adxl;
    record.accel_y_g.value = accy_g;
    record.accel_y_g.valid = status_adxl;
    record.accel_z_g.value = accz_g;
    record.accel_z_g.valid = status_adxl;
    record.gyro_x_dps.value = gyro_x_dps;
    record.gyro_x_dps.valid = status_imu;
    record.gyro_y_dps.value = gyro_y_dps;
    record.gyro_y_dps.valid = status_imu;
    record.gyro_z_dps.value = gyro_z_dps;
    record.gyro_z_dps.valid = status_imu;
    record.battery_v.value = battery_mv / 1000.0f;
    record.battery_v.valid = battery_mv != 0;
    record.temperature_c.value = temp_c;
    record.temperature_c.valid = status_sht;
    record.humidity_percent.value = hum;
    record.humidity_percent.valid = status_sht;
    record.pressure_pa.value = pressure_pa;
    record.pressure_pa.valid = baro_valid;
    record.flight_state.value = flight_state;
    record.flight_state.valid = true;
    record.error_code.value = error_code;
    record.error_code.valid = true;
    record.water_detected.value = water_detected;
    record.water_detected.valid = true;
    if (!sd_telemetry_store.append(record)) {
      sd_ready = false;
      emit_setup_diag("SD_WRITE_FAIL");
    } else {
      sd_telemetry_store.service(time_ms);
    }
  }

  ground_station_serial.write(frame, frame_length);

  // 簡易延遲，約 10Hz (實際飛行時不能用 delay)
  delay(100); 
}
