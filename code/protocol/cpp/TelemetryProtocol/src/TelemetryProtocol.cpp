#include "TelemetryProtocol.h"

#include <string.h>

namespace rocket {
namespace protocol {
namespace {

void writeU16(uint8_t *output, size_t offset, uint16_t value) {
  output[offset] = static_cast<uint8_t>(value & 0xFF);
  output[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(uint8_t *output, size_t offset, uint32_t value) {
  output[offset] = static_cast<uint8_t>(value & 0xFF);
  output[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  output[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  output[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void writeI16(uint8_t *output, size_t offset, int16_t value) {
  writeU16(output, offset, static_cast<uint16_t>(value));
}

void writeI32(uint8_t *output, size_t offset, int32_t value) {
  writeU32(output, offset, static_cast<uint32_t>(value));
}

uint16_t readU16(const uint8_t *input, size_t offset) {
  return static_cast<uint16_t>(input[offset]) |
         (static_cast<uint16_t>(input[offset + 1]) << 8);
}

bool valid(uint32_t flags, uint32_t flag) {
  return (flags & flag) != 0;
}

}  // namespace

TelemetryV2Fields::TelemetryV2Fields()
    : source_flags(0),
      boot_id(0),
      sequence(0),
      uptime_ms(0),
      valid_flags(0),
      latitude_e7(kInvalidI32),
      longitude_e7(kInvalidI32),
      gps_altitude_dm(kInvalidI16),
      gps_speed_dms(kInvalidI16),
      satellite_count(kInvalidU8),
      roll_cdeg(kInvalidI16),
      pitch_cdeg(kInvalidI16),
      heading_ddeg(kInvalidU16),
      gyro_x_ddeg_s(kInvalidI16),
      gyro_y_ddeg_s(kInvalidI16),
      gyro_z_ddeg_s(kInvalidI16),
      accel_x_cg(kInvalidI16),
      accel_y_cg(kInvalidI16),
      accel_z_cg(kInvalidI16),
      baro_altitude_dm(kInvalidI16),
      battery_mv(kInvalidU16),
      servo_power_mv(kInvalidU16),
      servo_angle_ddeg(kInvalidI16),
      flight_state(kInvalidU8),
      error_code(kInvalidU8),
      water_detected(kInvalidU8) {}

uint16_t crc16CcittFalse(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  if (data == 0) {
    return crc;
  }
  for (size_t index = 0; index < length; ++index) {
    crc ^= static_cast<uint16_t>(data[index]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000)
                ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

uint8_t telemetryV1Xor(const uint8_t *data, size_t length) {
  uint8_t value = 0;
  if (data == 0) {
    return value;
  }
  for (size_t index = 0; index < length; ++index) {
    value ^= data[index];
  }
  return value;
}

bool isValidTelemetryV1(const uint8_t *frame, size_t length) {
  return frame != 0 &&
         length == kTelemetryV1FrameLength &&
         frame[0] == 0x55 &&
         frame[1] == 0xAA &&
         telemetryV1Xor(frame, length - 1) == frame[length - 1];
}

bool isValidTelemetryV2(const uint8_t *frame, size_t length) {
  if (frame == 0 ||
      length != kTelemetryV2FrameLength ||
      frame[0] != kTelemetryV2Sync0 ||
      frame[1] != kTelemetryV2Sync1 ||
      frame[2] != kTelemetryV2Version ||
      frame[3] != kTelemetryV2FrameLength ||
      frame[4] != kTelemetryV2MessageType) {
    return false;
  }
  return crc16CcittFalse(frame, 62) == readU16(frame, 62);
}

bool encodeTelemetryV2(const TelemetryV2Fields &fields,
                       uint8_t *output,
                       size_t output_length) {
  if (output == 0 || output_length < kTelemetryV2FrameLength) {
    return false;
  }
  memset(output, 0, kTelemetryV2FrameLength);
  output[0] = kTelemetryV2Sync0;
  output[1] = kTelemetryV2Sync1;
  output[2] = kTelemetryV2Version;
  output[3] = kTelemetryV2FrameLength;
  output[4] = kTelemetryV2MessageType;
  output[5] = fields.source_flags;
  writeU16(output, 6, fields.boot_id);
  writeU32(output, 8, fields.sequence);
  writeU32(output, 12, fields.uptime_ms);
  writeU32(output, 16, fields.valid_flags);
  writeI32(output, 20,
           valid(fields.valid_flags, kValidGpsPosition)
               ? fields.latitude_e7
               : kInvalidI32);
  writeI32(output, 24,
           valid(fields.valid_flags, kValidGpsPosition)
               ? fields.longitude_e7
               : kInvalidI32);
  writeI16(output, 28,
           valid(fields.valid_flags, kValidGpsAltitude)
               ? fields.gps_altitude_dm
               : kInvalidI16);
  writeI16(output, 30,
           valid(fields.valid_flags, kValidGpsSpeed)
               ? fields.gps_speed_dms
               : kInvalidI16);
  output[32] = valid(fields.valid_flags, kValidSatelliteCount)
                   ? fields.satellite_count
                   : kInvalidU8;
  writeI16(output, 33,
           valid(fields.valid_flags, kValidAttitude)
               ? fields.roll_cdeg
               : kInvalidI16);
  writeI16(output, 35,
           valid(fields.valid_flags, kValidAttitude)
               ? fields.pitch_cdeg
               : kInvalidI16);
  writeU16(output, 37,
           valid(fields.valid_flags, kValidHeading)
               ? fields.heading_ddeg
               : kInvalidU16);
  writeI16(output, 39,
           valid(fields.valid_flags, kValidGyro)
               ? fields.gyro_x_ddeg_s
               : kInvalidI16);
  writeI16(output, 41,
           valid(fields.valid_flags, kValidGyro)
               ? fields.gyro_y_ddeg_s
               : kInvalidI16);
  writeI16(output, 43,
           valid(fields.valid_flags, kValidGyro)
               ? fields.gyro_z_ddeg_s
               : kInvalidI16);
  writeI16(output, 45,
           valid(fields.valid_flags, kValidAcceleration)
               ? fields.accel_x_cg
               : kInvalidI16);
  writeI16(output, 47,
           valid(fields.valid_flags, kValidAcceleration)
               ? fields.accel_y_cg
               : kInvalidI16);
  writeI16(output, 49,
           valid(fields.valid_flags, kValidAcceleration)
               ? fields.accel_z_cg
               : kInvalidI16);
  writeI16(output, 51,
           valid(fields.valid_flags, kValidBaroAltitude)
               ? fields.baro_altitude_dm
               : kInvalidI16);
  writeU16(output, 53,
           valid(fields.valid_flags, kValidBattery)
               ? fields.battery_mv
               : kInvalidU16);
  writeU16(output, 55,
           valid(fields.valid_flags, kValidServoPower)
               ? fields.servo_power_mv
               : kInvalidU16);
  writeI16(output, 57,
           valid(fields.valid_flags, kValidServoAngle)
               ? fields.servo_angle_ddeg
               : kInvalidI16);
  output[59] = valid(fields.valid_flags, kValidFlightState)
                   ? fields.flight_state
                   : kInvalidU8;
  output[60] = valid(fields.valid_flags, kValidErrorCode)
                   ? fields.error_code
                   : kInvalidU8;
  output[61] = valid(fields.valid_flags, kValidWater)
                   ? fields.water_detected
                   : kInvalidU8;
  writeU16(output, 62, crc16CcittFalse(output, 62));
  return true;
}

}  // namespace protocol
}  // namespace rocket
