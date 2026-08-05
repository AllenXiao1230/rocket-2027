#include <limits.h>
#include <unity.h>

#include "navigation/altitude_estimator.h"

using rocket::navigation::AltitudeEstimate;
using rocket::navigation::AltitudeEstimator;
using rocket::navigation::AltitudeInputs;
using rocket::navigation::AltitudeSource;

namespace {

AltitudeEstimate update(AltitudeEstimator &estimator,
                        uint32_t now_ms,
                        bool barometer_valid,
                        float barometer_altitude_m,
                        bool gps_valid,
                        float gps_altitude_m) {
  AltitudeInputs inputs;
  inputs.now_ms = now_ms;
  inputs.barometer_valid = barometer_valid;
  inputs.barometer_altitude_m = barometer_altitude_m;
  inputs.gps_valid = gps_valid;
  inputs.gps_altitude_m = gps_altitude_m;
  return estimator.update(inputs);
}

void testResetStartsInvalid() {
  AltitudeEstimator estimator;
  const AltitudeEstimate estimate = estimator.estimate();

  TEST_ASSERT_FALSE(estimate.altitude_valid);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AltitudeSource::kNone),
      static_cast<uint8_t>(estimate.altitude_source));
  TEST_ASSERT_FALSE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, estimate.vertical_speed_mps);
}

void testBarometerTakesPriorityAndGpsIsFallback() {
  AltitudeEstimator estimator;

  AltitudeEstimate estimate =
      update(estimator, 100, true, 123.5f, true, 987.0f);
  TEST_ASSERT_TRUE(estimate.altitude_valid);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AltitudeSource::kBarometer),
      static_cast<uint8_t>(estimate.altitude_source));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 123.5f, estimate.altitude_m);

  estimator.reset();
  estimate = update(estimator, 200, false, 0.0f, true, 456.25f);
  TEST_ASSERT_TRUE(estimate.altitude_valid);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AltitudeSource::kGps),
      static_cast<uint8_t>(estimate.altitude_source));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 456.25f, estimate.altitude_m);
}

void testVerticalSpeedUsesExistingLowPassFilter() {
  AltitudeEstimator estimator;

  update(estimator, 1000, true, 100.0f, false, 0.0f);
  AltitudeEstimate estimate =
      update(estimator, 1500, true, 110.0f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f,
                           estimate.vertical_speed_mps);

  estimate = update(estimator, 2000, true, 115.0f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 18.0f,
                           estimate.vertical_speed_mps);
}

void testVelocityIntervalBoundariesMatchCurrentBehavior() {
  AltitudeEstimator estimator;

  update(estimator, 0, true, 0.0f, false, 0.0f);
  AltitudeEstimate estimate =
      update(estimator, 10, true, 1.0f, false, 0.0f);
  TEST_ASSERT_FALSE(estimate.vertical_speed_valid);

  estimate = update(estimator, 21, true, 2.1f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f,
                           estimate.vertical_speed_mps);

  estimate = update(estimator, 1021, true, 102.1f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f,
                           estimate.vertical_speed_mps);
}

void testVerticalSpeedBecomesInvalidOnlyAfterStaleTimeout() {
  AltitudeEstimator estimator;

  update(estimator, 1000, true, 100.0f, false, 0.0f);
  AltitudeEstimate estimate =
      update(estimator, 1500, true, 110.0f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);

  estimate = update(estimator, 2500, false, 0.0f, false, 0.0f);
  TEST_ASSERT_FALSE(estimate.altitude_valid);
  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);

  estimate = update(estimator, 2501, false, 0.0f, false, 0.0f);
  TEST_ASSERT_FALSE(estimate.vertical_speed_valid);
}

void testElapsedTimeIsWrapSafe() {
  AltitudeEstimator estimator;
  const uint32_t before_wrap = UINT32_MAX - 199u;

  update(estimator, before_wrap, true, 100.0f, false, 0.0f);
  const AltitudeEstimate estimate =
      update(estimator, 100, true, 106.0f, false, 0.0f);

  TEST_ASSERT_TRUE(estimate.vertical_speed_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f,
                           estimate.vertical_speed_mps);
}

void testResetClearsHistoryAndFilteredVelocity() {
  AltitudeEstimator estimator;

  update(estimator, 1000, true, 100.0f, false, 0.0f);
  update(estimator, 1500, true, 110.0f, false, 0.0f);
  TEST_ASSERT_TRUE(estimator.estimate().vertical_speed_valid);

  estimator.reset();
  const AltitudeEstimate estimate =
      update(estimator, 2000, true, 130.0f, false, 0.0f);
  TEST_ASSERT_TRUE(estimate.altitude_valid);
  TEST_ASSERT_FALSE(estimate.vertical_speed_valid);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testResetStartsInvalid);
  RUN_TEST(testBarometerTakesPriorityAndGpsIsFallback);
  RUN_TEST(testVerticalSpeedUsesExistingLowPassFilter);
  RUN_TEST(testVelocityIntervalBoundariesMatchCurrentBehavior);
  RUN_TEST(testVerticalSpeedBecomesInvalidOnlyAfterStaleTimeout);
  RUN_TEST(testElapsedTimeIsWrapSafe);
  RUN_TEST(testResetClearsHistoryAndFilteredVelocity);
  return UNITY_END();
}
