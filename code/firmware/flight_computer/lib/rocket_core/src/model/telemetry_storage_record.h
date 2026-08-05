#pragma once

#include <stdint.h>

namespace rocket {
namespace model {

template <typename T>
struct ValidValue {
  T value;
  bool valid;
};

// Platform-independent snapshot consumed by the storage layer.
//
// Each value whose source can be unavailable carries explicit validity.
// Formatters must never infer validity from a numeric sentinel or from zero.
struct TelemetryStorageRecord {
  uint32_t time_ms;
  ValidValue<uint32_t> rtc_unix;
  ValidValue<double> latitude_deg;
  ValidValue<double> longitude_deg;
  ValidValue<float> gps_altitude_m;
  ValidValue<float> baro_altitude_m;
  ValidValue<float> speed_mps;
  ValidValue<float> heading_deg;
  ValidValue<uint8_t> satellite_count;
  ValidValue<float> roll_deg;
  ValidValue<float> pitch_deg;
  ValidValue<float> accel_x_g;
  ValidValue<float> accel_y_g;
  ValidValue<float> accel_z_g;
  ValidValue<float> gyro_x_dps;
  ValidValue<float> gyro_y_dps;
  ValidValue<float> gyro_z_dps;
  ValidValue<float> battery_v;
  ValidValue<float> servo_power_mv;
  ValidValue<float> servo_angle_deg;
  ValidValue<float> temperature_c;
  ValidValue<float> humidity_percent;
  ValidValue<float> pressure_pa;
  ValidValue<uint8_t> flight_state;
  ValidValue<uint8_t> error_code;
  ValidValue<uint8_t> water_detected;
};

}  // namespace model
}  // namespace rocket
