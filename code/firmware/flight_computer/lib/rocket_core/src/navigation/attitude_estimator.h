#pragma once

#include <stdint.h>

#include "model/attitude_types.h"

namespace rocket {
namespace navigation {

struct AttitudeEstimatorConfig {
  float madgwick_beta;
  float roll_pitch_filter_alpha;
  float vertical_heading_lock_pitch_deg;
  uint32_t maximum_update_interval_us;

  AttitudeEstimatorConfig()
      : madgwick_beta(0.15f),
        roll_pitch_filter_alpha(0.2f),
        vertical_heading_lock_pitch_deg(80.0f),
        maximum_update_interval_us(1000000u) {}
};

class AttitudeEstimator {
 public:
  explicit AttitudeEstimator(
      const AttitudeEstimatorConfig &config = AttitudeEstimatorConfig());

  void reset();
  AttitudeEstimate update(const AttitudeInputs &inputs);
  const AttitudeEstimate &estimate() const;

 private:
  static uint32_t elapsed(uint32_t now_us, uint32_t start_us);
  static float clampUnitInterval(float value);
  static float inverseSquareRoot(float value);
  static float normalizeDegrees(float degrees);

  void initializeQuaternion(float accel_x_g,
                            float accel_y_g,
                            float accel_z_g);
  void updateQuaternion(float gyro_x_rad_s,
                        float gyro_y_rad_s,
                        float gyro_z_rad_s,
                        float accel_x_g,
                        float accel_y_g,
                        float accel_z_g,
                        float dt_s);
  void updateEstimateFromQuaternion();

  AttitudeEstimatorConfig config_;
  AttitudeEstimate estimate_;

  bool fusion_initialized_;
  uint32_t last_update_us_;
  float q0_;
  float q1_;
  float q2_;
  float q3_;

  bool yaw_zero_initialized_;
  float yaw_zero_deg_;
  bool heading_hold_initialized_;
  float heading_hold_deg_;

  bool roll_pitch_filter_initialized_;
  float filtered_roll_deg_;
  float filtered_pitch_deg_;
};

}  // namespace navigation
}  // namespace rocket
