#include <stdint.h>
#include <unity.h>

#include "navigation/attitude_estimator.h"

using rocket::navigation::AttitudeEstimate;
using rocket::navigation::AttitudeEstimator;
using rocket::navigation::AttitudeEstimatorConfig;
using rocket::navigation::AttitudeInputs;

namespace {

AttitudeInputs sample(uint32_t now_us,
                      bool valid,
                      float ax,
                      float ay,
                      float az,
                      float gx,
                      float gy,
                      float gz) {
  AttitudeInputs inputs;
  inputs.now_us = now_us;
  inputs.imu_valid = valid;
  inputs.body_accel_x_g = ax;
  inputs.body_accel_y_g = ay;
  inputs.body_accel_z_g = az;
  inputs.body_gyro_x_dps = gx;
  inputs.body_gyro_y_dps = gy;
  inputs.body_gyro_z_dps = gz;
  return inputs;
}

void assertEstimateNear(const AttitudeEstimate &expected,
                        const AttitudeEstimate &actual,
                        float tolerance) {
  TEST_ASSERT_EQUAL(expected.attitude_valid, actual.attitude_valid);
  TEST_ASSERT_EQUAL(expected.heading_valid, actual.heading_valid);
  TEST_ASSERT_FLOAT_WITHIN(tolerance,
                           expected.roll_deg,
                           actual.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(tolerance,
                           expected.pitch_deg,
                           actual.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(tolerance,
                           expected.heading_deg,
                           actual.heading_deg);
}

void testResetStartsWithoutValidAttitudeOrHeading() {
  AttitudeEstimator estimator;
  const AttitudeEstimate estimate = estimator.estimate();

  TEST_ASSERT_FALSE(estimate.attitude_valid);
  TEST_ASSERT_FALSE(estimate.heading_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);
}

void testFirstValidLevelSampleInitializesAndZerosYaw() {
  AttitudeEstimator estimator;
  const AttitudeEstimate estimate = estimator.update(
      sample(0, true, 0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 0.0f));

  TEST_ASSERT_TRUE(estimate.attitude_valid);
  TEST_ASSERT_TRUE(estimate.heading_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);
}

void testFirstTiltedSampleUsesAccelerometerInitialization() {
  AttitudeEstimator estimator;
  const AttitudeEstimate estimate = estimator.update(
      sample(1000, true, 0.0f, 1.0f, 0.0f,
             0.0f, 0.0f, 0.0f));

  TEST_ASSERT_TRUE(estimate.attitude_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, estimate.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, estimate.heading_deg);
}

void testDefaultRollPitchFilterUsesAlphaPointTwo() {
  AttitudeEstimator filtered;
  AttitudeEstimatorConfig unfiltered_config;
  unfiltered_config.roll_pitch_filter_alpha = 1.0f;
  AttitudeEstimator unfiltered(unfiltered_config);

  const AttitudeInputs initial =
      sample(100000, true, 0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 0.0f);
  filtered.update(initial);
  unfiltered.update(initial);

  const AttitudeInputs changed =
      sample(200000, true, 0.20f, 0.10f, 0.97f,
             35.0f, -20.0f, 25.0f);
  const AttitudeEstimate filtered_estimate = filtered.update(changed);
  const AttitudeEstimate raw_estimate = unfiltered.update(changed);

  TEST_ASSERT_FLOAT_WITHIN(
      0.0005f,
      raw_estimate.roll_deg * 0.2f,
      filtered_estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(
      0.0005f,
      raw_estimate.pitch_deg * 0.2f,
      filtered_estimate.pitch_deg);
}

void testZeroAccelerationRetainsExistingQuaternion() {
  AttitudeEstimator estimator;
  estimator.update(sample(100000, true,
                          0.0f, 0.0f, 1.0f,
                          0.0f, 0.0f, 0.0f));
  const AttitudeEstimate before = estimator.estimate();

  const AttitudeEstimate after = estimator.update(
      sample(200000, true,
             0.0f, 0.0f, 0.0f,
             100.0f, 100.0f, 100.0f));

  assertEstimateNear(before, after, 0.0001f);
}

void testOneSecondOrLongerIntervalIsSkipped() {
  AttitudeEstimator estimator;
  estimator.update(sample(100000, true,
                          0.0f, 0.0f, 1.0f,
                          0.0f, 0.0f, 0.0f));
  const AttitudeEstimate before = estimator.estimate();

  const AttitudeEstimate after = estimator.update(
      sample(1100000, true,
             0.20f, 0.10f, 0.97f,
             35.0f, -20.0f, 25.0f));

  assertEstimateNear(before, after, 0.0001f);
}

void testMicrosWrapProducesSameResultAsNormalElapsedTime() {
  AttitudeEstimator normal;
  AttitudeEstimator wrapped;

  const AttitudeInputs initial_normal =
      sample(100000, true, 0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 0.0f);
  const uint32_t wrapped_start = UINT32_MAX - 50000u;
  const AttitudeInputs initial_wrapped =
      sample(wrapped_start, true, 0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 0.0f);
  normal.update(initial_normal);
  wrapped.update(initial_wrapped);

  const AttitudeInputs next_normal =
      sample(200000, true, 0.20f, 0.10f, 0.97f,
             35.0f, -20.0f, 25.0f);
  const AttitudeInputs next_wrapped =
      sample(49999u, true, 0.20f, 0.10f, 0.97f,
             35.0f, -20.0f, 25.0f);

  const AttitudeEstimate expected = normal.update(next_normal);
  const AttitudeEstimate actual = wrapped.update(next_wrapped);
  assertEstimateNear(expected, actual, 0.0001f);
}

void testHeadingIsHeldWhilePitchIsNearVertical() {
  AttitudeEstimator estimator;
  AttitudeEstimate estimate = estimator.update(
      sample(100000, true, -1.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f));
  TEST_ASSERT_TRUE(estimate.pitch_deg > 80.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);

  estimate = estimator.update(
      sample(101000, true, -1.0f, 0.05f, 0.0f,
             0.0f, 0.0f, 90.0f));
  TEST_ASSERT_TRUE(estimate.pitch_deg > 80.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);
}

void testTemporaryInvalidInputRetainsInitializedValidity() {
  AttitudeEstimator estimator;
  estimator.update(sample(100000, true,
                          0.0f, 0.0f, 1.0f,
                          0.0f, 0.0f, 0.0f));

  const AttitudeEstimate estimate = estimator.update(
      sample(200000, false,
             0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f));

  TEST_ASSERT_TRUE(estimate.attitude_valid);
  TEST_ASSERT_TRUE(estimate.heading_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);
}

void testResetClearsQuaternionYawZeroAndFilters() {
  AttitudeEstimator estimator;
  estimator.update(sample(100000, true,
                          0.0f, 1.0f, 0.0f,
                          0.0f, 0.0f, 0.0f));
  TEST_ASSERT_TRUE(estimator.estimate().attitude_valid);

  estimator.reset();
  const AttitudeEstimate estimate = estimator.update(
      sample(200000, true,
             0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.roll_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.pitch_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.heading_deg);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testResetStartsWithoutValidAttitudeOrHeading);
  RUN_TEST(testFirstValidLevelSampleInitializesAndZerosYaw);
  RUN_TEST(testFirstTiltedSampleUsesAccelerometerInitialization);
  RUN_TEST(testDefaultRollPitchFilterUsesAlphaPointTwo);
  RUN_TEST(testZeroAccelerationRetainsExistingQuaternion);
  RUN_TEST(testOneSecondOrLongerIntervalIsSkipped);
  RUN_TEST(testMicrosWrapProducesSameResultAsNormalElapsedTime);
  RUN_TEST(testHeadingIsHeldWhilePitchIsNearVertical);
  RUN_TEST(testTemporaryInvalidInputRetainsInitializedValidity);
  RUN_TEST(testResetClearsQuaternionYawZeroAndFilters);
  return UNITY_END();
}
