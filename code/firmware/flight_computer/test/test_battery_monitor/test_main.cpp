#include <unity.h>

#include "drivers/battery_monitor.h"

using rocket::drivers::BatteryMonitor;

namespace {

void testZeroAdcIsInvalid() {
  TEST_ASSERT_EQUAL_UINT16(
      0, BatteryMonitor::scaleAdcMillivolts(0, 200000.0f, 100000.0f));
}

void testDividerScaling() {
  TEST_ASSERT_EQUAL_UINT16(
      12000,
      BatteryMonitor::scaleAdcMillivolts(4000, 200000.0f, 100000.0f));
}

void testInvalidDividerFailsClosed() {
  TEST_ASSERT_EQUAL_UINT16(
      0, BatteryMonitor::scaleAdcMillivolts(1000, 200000.0f, 0.0f));
}

void testScalingSaturates() {
  TEST_ASSERT_EQUAL_UINT16(
      UINT16_MAX,
      BatteryMonitor::scaleAdcMillivolts(
          65535, 200000.0f, 100000.0f));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testZeroAdcIsInvalid);
  RUN_TEST(testDividerScaling);
  RUN_TEST(testInvalidDividerFailsClosed);
  RUN_TEST(testScalingSaturates);
  return UNITY_END();
}
