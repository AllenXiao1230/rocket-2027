#include "navigation/attitude_estimator.h"

#include <math.h>

namespace rocket {
namespace navigation {
namespace {

const float kDegreesToRadians = 0.0174532925f;
const float kRadiansToDegrees = 57.2957795f;

}  // namespace

AttitudeEstimator::AttitudeEstimator(const AttitudeEstimatorConfig &config)
    : config_(config) {
  config_.roll_pitch_filter_alpha =
      clampUnitInterval(config_.roll_pitch_filter_alpha);
  reset();
}

void AttitudeEstimator::reset() {
  estimate_ = AttitudeEstimate();

  fusion_initialized_ = false;
  last_update_us_ = 0;
  q0_ = 1.0f;
  q1_ = 0.0f;
  q2_ = 0.0f;
  q3_ = 0.0f;

  yaw_zero_initialized_ = false;
  yaw_zero_deg_ = 0.0f;
  heading_hold_initialized_ = false;
  heading_hold_deg_ = 0.0f;

  roll_pitch_filter_initialized_ = false;
  filtered_roll_deg_ = 0.0f;
  filtered_pitch_deg_ = 0.0f;
}

AttitudeEstimate AttitudeEstimator::update(const AttitudeInputs &inputs) {
  if (inputs.imu_valid) {
    if (!fusion_initialized_) {
      initializeQuaternion(inputs.body_accel_x_g,
                           inputs.body_accel_y_g,
                           inputs.body_accel_z_g);
      last_update_us_ = inputs.now_us;
      fusion_initialized_ = true;
    } else {
      const uint32_t delta_us = elapsed(inputs.now_us, last_update_us_);
      if (delta_us > 0 &&
          delta_us < config_.maximum_update_interval_us) {
        const float dt_s =
            static_cast<float>(delta_us) / 1000000.0f;
        updateQuaternion(
            inputs.body_gyro_x_dps * kDegreesToRadians,
            inputs.body_gyro_y_dps * kDegreesToRadians,
            inputs.body_gyro_z_dps * kDegreesToRadians,
            inputs.body_accel_x_g,
            inputs.body_accel_y_g,
            inputs.body_accel_z_g,
            dt_s);
      }
      // Preserve the existing loop behavior: even a skipped interval becomes
      // the new timestamp, so the next valid sample starts a fresh interval.
      last_update_us_ = inputs.now_us;
    }
  }

  if (fusion_initialized_) {
    updateEstimateFromQuaternion();
  }
  return estimate_;
}

const AttitudeEstimate &AttitudeEstimator::estimate() const {
  return estimate_;
}

uint32_t AttitudeEstimator::elapsed(uint32_t now_us, uint32_t start_us) {
  return now_us - start_us;
}

float AttitudeEstimator::clampUnitInterval(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float AttitudeEstimator::inverseSquareRoot(float value) {
  return 1.0f / sqrtf(value);
}

float AttitudeEstimator::normalizeDegrees(float degrees) {
  if (degrees < 0.0f) {
    degrees += 360.0f;
  }
  if (degrees >= 360.0f) {
    degrees -= 360.0f;
  }
  return degrees;
}

void AttitudeEstimator::initializeQuaternion(float accel_x_g,
                                             float accel_y_g,
                                             float accel_z_g) {
  const float roll_acc = atan2f(accel_y_g, accel_z_g);
  const float pitch_acc =
      atan2f(-accel_x_g,
             sqrtf(accel_y_g * accel_y_g +
                   accel_z_g * accel_z_g));
  const float cr = cosf(roll_acc * 0.5f);
  const float sr = sinf(roll_acc * 0.5f);
  const float cp = cosf(pitch_acc * 0.5f);
  const float sp = sinf(pitch_acc * 0.5f);
  q0_ = cr * cp;
  q1_ = sr * cp;
  q2_ = cr * sp;
  q3_ = -sr * sp;
}

void AttitudeEstimator::updateQuaternion(float gyro_x_rad_s,
                                         float gyro_y_rad_s,
                                         float gyro_z_rad_s,
                                         float accel_x_g,
                                         float accel_y_g,
                                         float accel_z_g,
                                         float dt_s) {
  // This intentionally mirrors the Madgwick implementation formerly embedded
  // in main.cpp, including retaining the previous quaternion when the
  // accelerometer vector or corrective gradient is zero.
  if (accel_x_g == 0.0f &&
      accel_y_g == 0.0f &&
      accel_z_g == 0.0f) {
    return;
  }

  float reciprocal_norm = inverseSquareRoot(
      accel_x_g * accel_x_g +
      accel_y_g * accel_y_g +
      accel_z_g * accel_z_g);
  accel_x_g *= reciprocal_norm;
  accel_y_g *= reciprocal_norm;
  accel_z_g *= reciprocal_norm;

  const float two_q0 = 2.0f * q0_;
  const float two_q1 = 2.0f * q1_;
  const float two_q2 = 2.0f * q2_;
  const float two_q3 = 2.0f * q3_;
  const float four_q0 = 4.0f * q0_;
  const float four_q1 = 4.0f * q1_;
  const float four_q2 = 4.0f * q2_;
  const float eight_q1 = 8.0f * q1_;
  const float eight_q2 = 8.0f * q2_;
  const float q0_squared = q0_ * q0_;
  const float q1_squared = q1_ * q1_;
  const float q2_squared = q2_ * q2_;
  const float q3_squared = q3_ * q3_;

  float s0 = four_q0 * q2_squared +
             two_q2 * accel_x_g +
             four_q0 * q1_squared -
             two_q1 * accel_y_g;
  float s1 = four_q1 * q3_squared -
             two_q3 * accel_x_g +
             4.0f * q0_squared * q1_ -
             two_q0 * accel_y_g -
             four_q1 +
             eight_q1 * q1_squared +
             eight_q1 * q2_squared +
             four_q1 * accel_z_g;
  float s2 = 4.0f * q0_squared * q2_ +
             two_q0 * accel_x_g +
             four_q2 * q3_squared -
             two_q3 * accel_y_g -
             four_q2 +
             eight_q2 * q1_squared +
             eight_q2 * q2_squared +
             four_q2 * accel_z_g;
  float s3 = 4.0f * q1_squared * q3_ -
             two_q1 * accel_x_g +
             4.0f * q2_squared * q3_ -
             two_q2 * accel_y_g;

  const float correction_norm =
      s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
  if (correction_norm == 0.0f) {
    return;
  }
  reciprocal_norm = inverseSquareRoot(correction_norm);
  s0 *= reciprocal_norm;
  s1 *= reciprocal_norm;
  s2 *= reciprocal_norm;
  s3 *= reciprocal_norm;

  const float q_dot1 =
      0.5f * (-q1_ * gyro_x_rad_s -
              q2_ * gyro_y_rad_s -
              q3_ * gyro_z_rad_s) -
      config_.madgwick_beta * s0;
  const float q_dot2 =
      0.5f * (q0_ * gyro_x_rad_s +
              q2_ * gyro_z_rad_s -
              q3_ * gyro_y_rad_s) -
      config_.madgwick_beta * s1;
  const float q_dot3 =
      0.5f * (q0_ * gyro_y_rad_s -
              q1_ * gyro_z_rad_s +
              q3_ * gyro_x_rad_s) -
      config_.madgwick_beta * s2;
  const float q_dot4 =
      0.5f * (q0_ * gyro_z_rad_s +
              q1_ * gyro_y_rad_s -
              q2_ * gyro_x_rad_s) -
      config_.madgwick_beta * s3;

  q0_ += q_dot1 * dt_s;
  q1_ += q_dot2 * dt_s;
  q2_ += q_dot3 * dt_s;
  q3_ += q_dot4 * dt_s;

  reciprocal_norm = inverseSquareRoot(
      q0_ * q0_ + q1_ * q1_ + q2_ * q2_ + q3_ * q3_);
  q0_ *= reciprocal_norm;
  q1_ *= reciprocal_norm;
  q2_ *= reciprocal_norm;
  q3_ *= reciprocal_norm;
}

void AttitudeEstimator::updateEstimateFromQuaternion() {
  const float sin_roll =
      2.0f * (q0_ * q1_ + q2_ * q3_);
  const float cos_roll =
      1.0f - 2.0f * (q1_ * q1_ + q2_ * q2_);
  const float raw_roll_deg =
      atan2f(sin_roll, cos_roll) * kRadiansToDegrees;

  float sin_pitch =
      2.0f * (q0_ * q2_ - q3_ * q1_);
  if (sin_pitch > 1.0f) {
    sin_pitch = 1.0f;
  }
  if (sin_pitch < -1.0f) {
    sin_pitch = -1.0f;
  }
  const float raw_pitch_deg =
      asinf(sin_pitch) * kRadiansToDegrees;

  const float sin_yaw =
      2.0f * (q0_ * q3_ + q1_ * q2_);
  const float cos_yaw =
      1.0f - 2.0f * (q2_ * q2_ + q3_ * q3_);
  float yaw_deg =
      normalizeDegrees(atan2f(sin_yaw, cos_yaw) *
                       kRadiansToDegrees);

  if (!yaw_zero_initialized_) {
    yaw_zero_deg_ = yaw_deg;
    yaw_zero_initialized_ = true;
  }
  yaw_deg = normalizeDegrees(yaw_deg - yaw_zero_deg_);

  if (!heading_hold_initialized_) {
    heading_hold_deg_ = yaw_deg;
    heading_hold_initialized_ = true;
  }
  if (fabsf(raw_pitch_deg) <
      config_.vertical_heading_lock_pitch_deg) {
    heading_hold_deg_ = yaw_deg;
  }

  if (!roll_pitch_filter_initialized_) {
    filtered_roll_deg_ = raw_roll_deg;
    filtered_pitch_deg_ = raw_pitch_deg;
    roll_pitch_filter_initialized_ = true;
  } else {
    filtered_roll_deg_ +=
        config_.roll_pitch_filter_alpha *
        (raw_roll_deg - filtered_roll_deg_);
    filtered_pitch_deg_ +=
        config_.roll_pitch_filter_alpha *
        (raw_pitch_deg - filtered_pitch_deg_);
  }

  estimate_.attitude_valid = true;
  estimate_.heading_valid = true;
  estimate_.roll_deg = filtered_roll_deg_;
  estimate_.pitch_deg = filtered_pitch_deg_;
  estimate_.heading_deg = heading_hold_deg_;
}

}  // namespace navigation
}  // namespace rocket
