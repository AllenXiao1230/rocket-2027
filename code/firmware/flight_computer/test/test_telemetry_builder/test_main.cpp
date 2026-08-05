#include <limits>
#include <stdint.h>

#include <unity.h>

#include "model/telemetry_sample.h"
#include "telemetry/telemetry_builder.h"

using rocket::model::HeadingTelemetrySource;
using rocket::model::StateAltitudeTelemetrySource;
using rocket::model::TelemetrySample;
using rocket::telemetry::TelemetryBuilder;

namespace {

TelemetrySample nominalSample() {
  TelemetrySample sample;
  sample.context.boot_id = 0xBEEF;
  sample.context.sequence = 0x12345678;
  sample.context.uptime_ms = 12345;

  sample.gps.position_valid = true;
  sample.gps.latitude_deg = 25.0123456;
  sample.gps.longitude_deg = 121.9876543;
  sample.gps.altitude_valid = true;
  sample.gps.altitude_m = 123.4f;
  sample.gps.speed_valid = true;
  sample.gps.speed_mps = 8.7f;
  sample.gps.satellite_count_valid = true;
  sample.gps.satellite_count = 9;

  sample.navigation.attitude_valid = true;
  sample.navigation.roll_deg = -12.34f;
  sample.navigation.pitch_deg = 5.67f;
  sample.navigation.heading_valid = true;
  sample.navigation.heading_deg = 123.4f;
  sample.navigation.heading_source = HeadingTelemetrySource::kImu;
  sample.navigation.state_altitude_valid = true;
  sample.navigation.state_altitude_source =
      StateAltitudeTelemetrySource::kBarometer;

  sample.motion.gyro_valid = true;
  sample.motion.gyro_x_dps = -1.2f;
  sample.motion.gyro_y_dps = 3.4f;
  sample.motion.gyro_z_dps = -5.6f;
  sample.motion.acceleration_valid = true;
  sample.motion.acceleration_x_g = 3.21f;
  sample.motion.acceleration_y_g = -6.54f;
  sample.motion.acceleration_z_g = 9.87f;

  sample.environment.barometric_altitude_valid = true;
  sample.environment.barometric_altitude_m = 432.1f;

  sample.power.battery_valid = true;
  sample.power.battery_v = 11.85f;
  sample.power.servo_power_valid = true;
  sample.power.servo_power_v = 5.1f;
  sample.power.servo_angle_valid = true;
  sample.power.servo_angle_deg = -12.0f;

  sample.flight.state_valid = true;
  sample.flight.flight_state = 5;
  sample.flight.error_code_valid = true;
  sample.flight.error_code = 0x12;
  sample.flight.water_valid = true;
  sample.flight.water_detected = true;
  return sample;
}

void testNominalSampleMapsEveryFieldAndEncodes() {
  const rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(nominalSample());

  TEST_ASSERT_EQUAL_HEX16(0xBEEF, fields.boot_id);
  TEST_ASSERT_EQUAL_HEX32(0x12345678, fields.sequence);
  TEST_ASSERT_EQUAL_UINT32(12345, fields.uptime_ms);
  TEST_ASSERT_EQUAL_HEX32(0x00007FFF, fields.valid_flags);
  TEST_ASSERT_EQUAL_HEX8(0x05, fields.source_flags);
  TEST_ASSERT_EQUAL_INT32(250123456, fields.latitude_e7);
  TEST_ASSERT_EQUAL_INT32(1219876543, fields.longitude_e7);
  TEST_ASSERT_EQUAL_INT16(1234, fields.gps_altitude_dm);
  TEST_ASSERT_EQUAL_INT16(87, fields.gps_speed_dms);
  TEST_ASSERT_EQUAL_UINT8(9, fields.satellite_count);
  TEST_ASSERT_EQUAL_INT16(-1234, fields.roll_cdeg);
  TEST_ASSERT_EQUAL_INT16(567, fields.pitch_cdeg);
  TEST_ASSERT_EQUAL_UINT16(1234, fields.heading_ddeg);
  TEST_ASSERT_EQUAL_INT16(-12, fields.gyro_x_ddeg_s);
  TEST_ASSERT_EQUAL_INT16(34, fields.gyro_y_ddeg_s);
  TEST_ASSERT_EQUAL_INT16(-56, fields.gyro_z_ddeg_s);
  TEST_ASSERT_EQUAL_INT16(321, fields.accel_x_cg);
  TEST_ASSERT_EQUAL_INT16(-654, fields.accel_y_cg);
  TEST_ASSERT_EQUAL_INT16(987, fields.accel_z_cg);
  TEST_ASSERT_EQUAL_INT16(4321, fields.baro_altitude_dm);
  TEST_ASSERT_EQUAL_UINT16(11850, fields.battery_mv);
  TEST_ASSERT_EQUAL_UINT16(5100, fields.servo_power_mv);
  TEST_ASSERT_EQUAL_INT16(-120, fields.servo_angle_ddeg);
  TEST_ASSERT_EQUAL_UINT8(5, fields.flight_state);
  TEST_ASSERT_EQUAL_HEX8(0x12, fields.error_code);
  TEST_ASSERT_EQUAL_UINT8(1, fields.water_detected);

  uint8_t frame[rocket::protocol::kTelemetryV2FrameLength] = {};
  TEST_ASSERT_TRUE(rocket::protocol::encodeTelemetryV2(
      fields, frame, sizeof(frame)));
  TEST_ASSERT_TRUE(rocket::protocol::isValidTelemetryV2(
      frame, sizeof(frame)));
}

void testGpsValidityGroupsAreIndependent() {
  TelemetrySample sample;
  sample.gps.altitude_valid = true;
  sample.gps.altitude_m = 321.0f;
  sample.gps.satellite_count_valid = true;
  sample.gps.satellite_count = 7;

  const rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(sample);

  TEST_ASSERT_EQUAL_HEX32(
      rocket::protocol::kValidGpsAltitude |
          rocket::protocol::kValidSatelliteCount,
      fields.valid_flags);
  TEST_ASSERT_EQUAL_INT32(
      rocket::protocol::kInvalidI32, fields.latitude_e7);
  TEST_ASSERT_EQUAL_INT16(3210, fields.gps_altitude_dm);
  TEST_ASSERT_EQUAL_INT16(
      rocket::protocol::kInvalidI16, fields.gps_speed_dms);
  TEST_ASSERT_EQUAL_UINT8(7, fields.satellite_count);
}

void testInvalidNumericGroupsRemainSentinels() {
  TelemetrySample sample;
  sample.gps.position_valid = true;
  sample.gps.latitude_deg =
      std::numeric_limits<double>::quiet_NaN();
  sample.gps.longitude_deg = 121.0;
  sample.gps.speed_valid = true;
  sample.gps.speed_mps = -1.0f;
  sample.navigation.attitude_valid = true;
  sample.navigation.roll_deg =
      std::numeric_limits<float>::quiet_NaN();
  sample.navigation.pitch_deg = 1.0f;
  sample.navigation.heading_valid = true;
  sample.navigation.heading_deg = 10.0f;
  sample.navigation.heading_source = HeadingTelemetrySource::kNone;
  sample.motion.gyro_valid = true;
  sample.motion.gyro_x_dps =
      std::numeric_limits<float>::infinity();
  sample.motion.gyro_y_dps = 0.0f;
  sample.motion.gyro_z_dps = 0.0f;
  sample.power.battery_valid = true;
  sample.power.battery_v = -0.1f;

  const rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(sample);

  TEST_ASSERT_EQUAL_HEX32(0, fields.valid_flags);
  TEST_ASSERT_EQUAL_HEX8(0, fields.source_flags);
  TEST_ASSERT_EQUAL_INT32(
      rocket::protocol::kInvalidI32, fields.latitude_e7);
  TEST_ASSERT_EQUAL_INT16(
      rocket::protocol::kInvalidI16, fields.gps_speed_dms);
  TEST_ASSERT_EQUAL_INT16(
      rocket::protocol::kInvalidI16, fields.roll_cdeg);
  TEST_ASSERT_EQUAL_UINT16(
      rocket::protocol::kInvalidU16, fields.heading_ddeg);
  TEST_ASSERT_EQUAL_INT16(
      rocket::protocol::kInvalidI16, fields.gyro_x_ddeg_s);
  TEST_ASSERT_EQUAL_UINT16(
      rocket::protocol::kInvalidU16, fields.battery_mv);
}

void testScalingClampsWithoutUsingReservedSentinels() {
  TelemetrySample sample;
  sample.gps.altitude_valid = true;
  sample.gps.altitude_m = -1000000.0f;
  sample.gps.speed_valid = true;
  sample.gps.speed_mps = 1000000.0f;
  sample.gps.satellite_count_valid = true;
  sample.gps.satellite_count = 65535;
  sample.navigation.heading_valid = true;
  sample.navigation.heading_deg = -0.1f;
  sample.navigation.heading_source = HeadingTelemetrySource::kGps;
  sample.motion.acceleration_valid = true;
  sample.motion.acceleration_x_g = -1000000.0f;
  sample.motion.acceleration_y_g = 0.0f;
  sample.motion.acceleration_z_g = 1000000.0f;
  sample.power.battery_valid = true;
  sample.power.battery_v = 1000000.0f;
  sample.power.servo_power_valid = true;
  sample.power.servo_power_v = 1000000.0f;
  sample.power.servo_angle_valid = true;
  sample.power.servo_angle_deg = -1000000.0f;

  const rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(sample);

  TEST_ASSERT_EQUAL_INT16(INT16_MIN + 1, fields.gps_altitude_dm);
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, fields.gps_speed_dms);
  TEST_ASSERT_EQUAL_UINT8(UINT8_MAX - 1, fields.satellite_count);
  TEST_ASSERT_EQUAL_UINT16(3599, fields.heading_ddeg);
  TEST_ASSERT_EQUAL_INT16(INT16_MIN + 1, fields.accel_x_cg);
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, fields.accel_z_cg);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX - 1, fields.battery_mv);
  TEST_ASSERT_EQUAL_UINT16(
      UINT16_MAX - 1, fields.servo_power_mv);
  TEST_ASSERT_EQUAL_INT16(INT16_MIN + 1, fields.servo_angle_ddeg);
}

void testSourceFlagsRequireValidMeasurements() {
  TelemetrySample sample;
  sample.navigation.heading_valid = false;
  sample.navigation.heading_deg = 45.0f;
  sample.navigation.heading_source = HeadingTelemetrySource::kImu;
  sample.navigation.state_altitude_valid = true;
  sample.navigation.state_altitude_source =
      StateAltitudeTelemetrySource::kBarometer;

  rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(sample);
  TEST_ASSERT_EQUAL_HEX8(0, fields.source_flags);

  sample.navigation.heading_valid = true;
  sample.environment.barometric_altitude_valid = true;
  sample.environment.barometric_altitude_m = 100.0f;
  fields = TelemetryBuilder().build(sample);

  TEST_ASSERT_EQUAL_HEX8(
      rocket::protocol::kSourceHeadingImu |
          rocket::protocol::kSourceStateAltitudeBaro,
      fields.source_flags);
}

void testFlightValidityAndWaterMapping() {
  TelemetrySample sample;
  sample.flight.state_valid = true;
  sample.flight.flight_state = 99;
  sample.flight.error_code_valid = true;
  sample.flight.error_code = 0xFF;
  sample.flight.water_valid = true;
  sample.flight.water_detected = false;

  const rocket::protocol::TelemetryV2Fields fields =
      TelemetryBuilder().build(sample);

  TEST_ASSERT_EQUAL_HEX32(
      rocket::protocol::kValidFlightState |
          rocket::protocol::kValidErrorCode |
          rocket::protocol::kValidWater,
      fields.valid_flags);
  TEST_ASSERT_EQUAL_UINT8(99, fields.flight_state);
  TEST_ASSERT_EQUAL_HEX8(0xFF, fields.error_code);
  TEST_ASSERT_EQUAL_UINT8(0, fields.water_detected);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testNominalSampleMapsEveryFieldAndEncodes);
  RUN_TEST(testGpsValidityGroupsAreIndependent);
  RUN_TEST(testInvalidNumericGroupsRemainSentinels);
  RUN_TEST(testScalingClampsWithoutUsingReservedSentinels);
  RUN_TEST(testSourceFlagsRequireValidMeasurements);
  RUN_TEST(testFlightValidityAndWaterMapping);
  return UNITY_END();
}
