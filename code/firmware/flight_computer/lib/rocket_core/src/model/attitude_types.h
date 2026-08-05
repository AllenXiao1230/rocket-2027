#pragma once

#include <stdint.h>

namespace rocket {
namespace navigation {

// Acceleration and angular rate must already be mapped into the body axes
// expected by the flight computer. Hardware/sensor axis mapping deliberately
// remains outside the estimator.
struct AttitudeInputs {
  uint32_t now_us;
  bool imu_valid;
  float body_accel_x_g;
  float body_accel_y_g;
  float body_accel_z_g;
  float body_gyro_x_dps;
  float body_gyro_y_dps;
  float body_gyro_z_dps;

  AttitudeInputs()
      : now_us(0),
        imu_valid(false),
        body_accel_x_g(0.0f),
        body_accel_y_g(0.0f),
        body_accel_z_g(0.0f),
        body_gyro_x_dps(0.0f),
        body_gyro_y_dps(0.0f),
        body_gyro_z_dps(0.0f) {}
};

struct AttitudeEstimate {
  // Valid means that the estimator has initialized a solution. As in the
  // existing firmware, a temporary invalid IMU sample retains the last
  // initialized solution; freshness remains a separate sensor-health concern.
  bool attitude_valid;
  bool heading_valid;
  float roll_deg;
  float pitch_deg;
  float heading_deg;

  AttitudeEstimate()
      : attitude_valid(false),
        heading_valid(false),
        roll_deg(0.0f),
        pitch_deg(0.0f),
        heading_deg(0.0f) {}
};

}  // namespace navigation
}  // namespace rocket
