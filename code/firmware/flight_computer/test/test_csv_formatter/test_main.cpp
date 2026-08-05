#include <math.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#include <unity.h>

#include "model/telemetry_storage_record.h"
#include "storage/csv_formatter.h"

using rocket::model::TelemetryStorageRecord;
using rocket::storage::formatTelemetryCsvHeader;
using rocket::storage::formatTelemetryCsvRow;

static_assert(std::is_standard_layout<TelemetryStorageRecord>::value,
              "TelemetryStorageRecord must remain standard-layout");
static_assert(std::is_trivial<TelemetryStorageRecord>::value,
              "TelemetryStorageRecord must remain trivial");

namespace {

template <typename T>
void setValid(rocket::model::ValidValue<T> &field, T value) {
  field.value = value;
  field.valid = true;
}

TelemetryStorageRecord nominalRecord() {
  TelemetryStorageRecord record = {};
  record.time_ms = 1234;
  setValid(record.rtc_unix, static_cast<uint32_t>(1700000000UL));
  setValid(record.latitude_deg, 25.125);
  setValid(record.longitude_deg, 121.5);
  setValid(record.gps_altitude_m, 123.5f);
  setValid(record.baro_altitude_m, 120.5f);
  setValid(record.speed_mps, 8.5f);
  setValid(record.heading_deg, 45.5f);
  setValid(record.satellite_count, static_cast<uint8_t>(9));
  setValid(record.roll_deg, -12.25f);
  setValid(record.pitch_deg, 5.5f);
  setValid(record.accel_x_g, 0.25f);
  setValid(record.accel_y_g, -0.5f);
  setValid(record.accel_z_g, 1.0f);
  setValid(record.gyro_x_dps, -1.5f);
  setValid(record.gyro_y_dps, 3.5f);
  setValid(record.gyro_z_dps, -5.5f);
  setValid(record.battery_v, 11.875f);
  setValid(record.servo_power_mv, 5100.0f);
  setValid(record.servo_angle_deg, 42.5f);
  setValid(record.temperature_c, 24.5f);
  setValid(record.humidity_percent, 62.5f);
  setValid(record.pressure_pa, 100125.0f);
  setValid(record.flight_state, static_cast<uint8_t>(5));
  setValid(record.error_code, static_cast<uint8_t>(18));
  setValid(record.water_detected, static_cast<uint8_t>(1));
  return record;
}

size_t commaCount(const char *text) {
  size_t count = 0;
  for (const char *cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor == ',') {
      ++count;
    }
  }
  return count;
}

void testHeaderIsFixedAndComplete() {
  char output[512] = {};
  size_t written = 0;

  TEST_ASSERT_TRUE(
      formatTelemetryCsvHeader(output, sizeof(output), &written));
  TEST_ASSERT_EQUAL_STRING(
      "time_ms,rtc,lat,lon,gps_alt,baro_alt,speed,heading,sat,roll,pitch,"
      "accx,accy,accz,gyro_x,gyro_y,gyro_z,battery,servo_power_mv,"
      "servo_angle_deg,temp,hum,pressure_pa,status,error,water\n",
      output);
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(strlen(output)),
      static_cast<uint32_t>(written));
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(rocket::storage::kTelemetryCsvColumnCount - 1),
      static_cast<uint32_t>(commaCount(output)));
}

void testNominalRowUsesStableColumnOrderAndPrecision() {
  const TelemetryStorageRecord record = nominalRecord();
  char output[512] = {};
  size_t written = 0;

  TEST_ASSERT_TRUE(formatTelemetryCsvRow(
      record, output, sizeof(output), &written));
  TEST_ASSERT_EQUAL_STRING(
      "1234,1700000000,25.1250000,121.5000000,123.5,120.5,8.5,45.5,9,"
      "-12.25,5.50,0.25,-0.50,1.00,-1.5,3.5,-5.5,11.875,5100,42.5,"
      "24.50,62.5,100125,5,18,1\n",
      output);
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(strlen(output)),
      static_cast<uint32_t>(written));
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(rocket::storage::kTelemetryCsvColumnCount - 1),
      static_cast<uint32_t>(commaCount(output)));
}

void testInvalidAndNonFiniteValuesBecomeEmptyFields() {
  TelemetryStorageRecord record = {};
  record.time_ms = 42;
  record.latitude_deg.valid = true;
  record.latitude_deg.value = NAN;
  record.battery_v.valid = true;
  record.battery_v.value = INFINITY;

  char output[128] = {};
  size_t written = 0;
  TEST_ASSERT_TRUE(formatTelemetryCsvRow(
      record, output, sizeof(output), &written));
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(rocket::storage::kTelemetryCsvColumnCount - 1),
      static_cast<uint32_t>(commaCount(output)));
  TEST_ASSERT_EQUAL_CHAR('4', output[0]);
  TEST_ASSERT_EQUAL_CHAR('2', output[1]);
  TEST_ASSERT_EQUAL_CHAR(',', output[2]);
  TEST_ASSERT_EQUAL_CHAR('\n', output[written - 1]);
  TEST_ASSERT_NULL(strstr(output, "nan"));
  TEST_ASSERT_NULL(strstr(output, "inf"));
}

void testOneByteShortBufferFailsWithoutCrossingCanaries() {
  const TelemetryStorageRecord record = nominalRecord();
  char reference[512] = {};
  size_t required = 0;
  TEST_ASSERT_TRUE(formatTelemetryCsvRow(
      record, reference, sizeof(reference), &required));

  char guarded[512];
  memset(guarded, '#', sizeof(guarded));
  size_t written = 999;
  TEST_ASSERT_FALSE(formatTelemetryCsvRow(
      record, &guarded[1], required, &written));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(written));
  TEST_ASSERT_EQUAL_CHAR('#', guarded[0]);
  TEST_ASSERT_EQUAL_CHAR('\0', guarded[1]);
  TEST_ASSERT_EQUAL_CHAR('#', guarded[required + 1]);
}

void testExactCapacitySucceedsWithoutCrossingCanaries() {
  const TelemetryStorageRecord record = nominalRecord();
  char reference[512] = {};
  size_t required = 0;
  TEST_ASSERT_TRUE(formatTelemetryCsvRow(
      record, reference, sizeof(reference), &required));

  char guarded[512];
  memset(guarded, '#', sizeof(guarded));
  size_t written = 0;
  TEST_ASSERT_TRUE(formatTelemetryCsvRow(
      record, &guarded[1], required + 1, &written));
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(required),
      static_cast<uint32_t>(written));
  TEST_ASSERT_EQUAL_CHAR('#', guarded[0]);
  TEST_ASSERT_EQUAL_CHAR('\0', guarded[required + 1]);
  TEST_ASSERT_EQUAL_CHAR('#', guarded[required + 2]);
  TEST_ASSERT_EQUAL_STRING(reference, &guarded[1]);
}

void testNullAndZeroCapacityAreRejected() {
  const TelemetryStorageRecord record = nominalRecord();
  size_t written = 123;
  TEST_ASSERT_FALSE(formatTelemetryCsvRow(record, nullptr, 0, &written));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(written));

  char output = '#';
  written = 123;
  TEST_ASSERT_FALSE(formatTelemetryCsvRow(record, &output, 0, &written));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(written));
  TEST_ASSERT_EQUAL_CHAR('#', output);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testHeaderIsFixedAndComplete);
  RUN_TEST(testNominalRowUsesStableColumnOrderAndPrecision);
  RUN_TEST(testInvalidAndNonFiniteValuesBecomeEmptyFields);
  RUN_TEST(testOneByteShortBufferFailsWithoutCrossingCanaries);
  RUN_TEST(testExactCapacitySucceedsWithoutCrossingCanaries);
  RUN_TEST(testNullAndZeroCapacityAreRejected);
  return UNITY_END();
}
