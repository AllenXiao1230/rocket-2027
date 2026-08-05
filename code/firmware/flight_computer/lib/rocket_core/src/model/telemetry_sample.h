#pragma once

#include <stdint.h>

namespace rocket {
namespace model {

enum class HeadingTelemetrySource : uint8_t {
  kNone = 0,
  kImu = 1,
  kGps = 2,
};

enum class StateAltitudeTelemetrySource : uint8_t {
  kNone = 0,
  kBarometer = 1,
  kGps = 2,
};

struct TelemetryContext {
  uint16_t boot_id;
  uint32_t sequence;
  uint32_t uptime_ms;

  TelemetryContext() : boot_id(0), sequence(0), uptime_ms(0) {}
};

struct GpsTelemetrySample {
  bool position_valid;
  double latitude_deg;
  double longitude_deg;

  bool altitude_valid;
  float altitude_m;

  bool speed_valid;
  float speed_mps;

  bool satellite_count_valid;
  uint16_t satellite_count;

  GpsTelemetrySample()
      : position_valid(false),
        latitude_deg(0.0),
        longitude_deg(0.0),
        altitude_valid(false),
        altitude_m(0.0f),
        speed_valid(false),
        speed_mps(0.0f),
        satellite_count_valid(false),
        satellite_count(0) {}
};

struct MotionTelemetrySample {
  bool gyro_valid;
  float gyro_x_dps;
  float gyro_y_dps;
  float gyro_z_dps;

  bool acceleration_valid;
  float acceleration_x_g;
  float acceleration_y_g;
  float acceleration_z_g;

  MotionTelemetrySample()
      : gyro_valid(false),
        gyro_x_dps(0.0f),
        gyro_y_dps(0.0f),
        gyro_z_dps(0.0f),
        acceleration_valid(false),
        acceleration_x_g(0.0f),
        acceleration_y_g(0.0f),
        acceleration_z_g(0.0f) {}
};

struct EnvironmentTelemetrySample {
  bool barometric_altitude_valid;
  float barometric_altitude_m;

  EnvironmentTelemetrySample()
      : barometric_altitude_valid(false),
        barometric_altitude_m(0.0f) {}
};

struct PowerTelemetrySample {
  bool battery_valid;
  float battery_v;

  bool servo_power_valid;
  float servo_power_v;

  bool servo_angle_valid;
  float servo_angle_deg;

  PowerTelemetrySample()
      : battery_valid(false),
        battery_v(0.0f),
        servo_power_valid(false),
        servo_power_v(0.0f),
        servo_angle_valid(false),
        servo_angle_deg(0.0f) {}
};

struct NavigationTelemetrySample {
  bool attitude_valid;
  float roll_deg;
  float pitch_deg;

  bool heading_valid;
  float heading_deg;
  HeadingTelemetrySource heading_source;

  bool state_altitude_valid;
  StateAltitudeTelemetrySource state_altitude_source;

  NavigationTelemetrySample()
      : attitude_valid(false),
        roll_deg(0.0f),
        pitch_deg(0.0f),
        heading_valid(false),
        heading_deg(0.0f),
        heading_source(HeadingTelemetrySource::kNone),
        state_altitude_valid(false),
        state_altitude_source(StateAltitudeTelemetrySource::kNone) {}
};

struct FlightTelemetrySample {
  bool state_valid;
  uint8_t flight_state;

  bool error_code_valid;
  uint8_t error_code;

  bool water_valid;
  bool water_detected;

  FlightTelemetrySample()
      : state_valid(false),
        flight_state(0),
        error_code_valid(false),
        error_code(0),
        water_valid(false),
        water_detected(false) {}
};

struct TelemetrySample {
  TelemetryContext context;
  GpsTelemetrySample gps;
  MotionTelemetrySample motion;
  EnvironmentTelemetrySample environment;
  PowerTelemetrySample power;
  NavigationTelemetrySample navigation;
  FlightTelemetrySample flight;
};

}  // namespace model
}  // namespace rocket
