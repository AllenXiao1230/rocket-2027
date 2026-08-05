#include <stdint.h>

#include <unity.h>

#include "TelemetryProtocol.h"

using rocket::protocol::TelemetryV2Fields;
using rocket::protocol::crc16CcittFalse;
using rocket::protocol::encodeTelemetryV2;
using rocket::protocol::isValidTelemetryV2;

namespace {

const uint8_t kExpectedNominalFrame[64] = {
    0xa5, 0x5a, 0x02, 0x40, 0x01, 0x05, 0xef, 0xbe,
    0x78, 0x56, 0x34, 0x12, 0x39, 0x30, 0x00, 0x00,
    0xff, 0x7f, 0x00, 0x00, 0xc0, 0x94, 0xe8, 0x0e,
    0xbf, 0xd6, 0xb5, 0x48, 0xd2, 0x04, 0x57, 0x00,
    0x09, 0x2e, 0xfb, 0x37, 0x02, 0xd2, 0x04, 0xf4,
    0xff, 0x22, 0x00, 0xc8, 0xff, 0x41, 0x01, 0x72,
    0xfd, 0xdb, 0x03, 0xe1, 0x10, 0x4a, 0x2e, 0xec,
    0x13, 0x88, 0xff, 0x05, 0x12, 0x01, 0x67, 0x68,
};

TelemetryV2Fields nominalFields() {
  TelemetryV2Fields fields;
  fields.source_flags = 5;
  fields.boot_id = 48879;
  fields.sequence = 305419896;
  fields.uptime_ms = 12345;
  fields.valid_flags = 32767;
  fields.latitude_e7 = 250123456;
  fields.longitude_e7 = 1219876543;
  fields.gps_altitude_dm = 1234;
  fields.gps_speed_dms = 87;
  fields.satellite_count = 9;
  fields.roll_cdeg = -1234;
  fields.pitch_cdeg = 567;
  fields.heading_ddeg = 1234;
  fields.gyro_x_ddeg_s = -12;
  fields.gyro_y_ddeg_s = 34;
  fields.gyro_z_ddeg_s = -56;
  fields.accel_x_cg = 321;
  fields.accel_y_cg = -654;
  fields.accel_z_cg = 987;
  fields.baro_altitude_dm = 4321;
  fields.battery_mv = 11850;
  fields.servo_power_mv = 5100;
  fields.servo_angle_ddeg = -120;
  fields.flight_state = 5;
  fields.error_code = 18;
  fields.water_detected = 1;
  return fields;
}

void testCrcStandardCheckValue() {
  const uint8_t input[] = {'1', '2', '3', '4', '5',
                           '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16CcittFalse(input, sizeof(input)));
}

void testCppEncoderMatchesSharedGoldenFrame() {
  uint8_t frame[64] = {};
  const TelemetryV2Fields fields = nominalFields();

  TEST_ASSERT_TRUE(encodeTelemetryV2(fields, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kExpectedNominalFrame, frame, sizeof(frame));
  TEST_ASSERT_TRUE(isValidTelemetryV2(frame, sizeof(frame)));
}

void testInvalidFieldsAreNormalizedToSentinels() {
  uint8_t frame[64] = {};
  TelemetryV2Fields fields = nominalFields();
  fields.valid_flags = rocket::protocol::kValidFlightState;

  TEST_ASSERT_TRUE(encodeTelemetryV2(fields, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[20]);
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[21]);
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[22]);
  TEST_ASSERT_EQUAL_HEX8(0x80, frame[23]);
  TEST_ASSERT_EQUAL_HEX8(0xff, frame[53]);
  TEST_ASSERT_EQUAL_HEX8(0xff, frame[54]);
  TEST_ASSERT_EQUAL_UINT8(fields.flight_state, frame[59]);
  TEST_ASSERT_TRUE(isValidTelemetryV2(frame, sizeof(frame)));
}

void testSingleBitCorruptionIsRejected() {
  uint8_t frame[64] = {};
  const TelemetryV2Fields fields = nominalFields();
  TEST_ASSERT_TRUE(encodeTelemetryV2(fields, frame, sizeof(frame)));

  frame[42] ^= 0x04;
  TEST_ASSERT_FALSE(isValidTelemetryV2(frame, sizeof(frame)));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testCrcStandardCheckValue);
  RUN_TEST(testCppEncoderMatchesSharedGoldenFrame);
  RUN_TEST(testInvalidFieldsAreNormalizedToSentinels);
  RUN_TEST(testSingleBitCorruptionIsRejected);
  return UNITY_END();
}
