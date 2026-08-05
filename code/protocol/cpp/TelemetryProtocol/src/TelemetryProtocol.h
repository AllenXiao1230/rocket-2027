#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TelemetryV2Generated.h"

namespace rocket {
namespace protocol {

struct TelemetryV2Fields {
  uint8_t source_flags;
  uint16_t boot_id;
  uint32_t sequence;
  uint32_t uptime_ms;
  uint32_t valid_flags;
  int32_t latitude_e7;
  int32_t longitude_e7;
  int16_t gps_altitude_dm;
  int16_t gps_speed_dms;
  uint8_t satellite_count;
  int16_t roll_cdeg;
  int16_t pitch_cdeg;
  uint16_t heading_ddeg;
  int16_t gyro_x_ddeg_s;
  int16_t gyro_y_ddeg_s;
  int16_t gyro_z_ddeg_s;
  int16_t accel_x_cg;
  int16_t accel_y_cg;
  int16_t accel_z_cg;
  int16_t baro_altitude_dm;
  uint16_t battery_mv;
  uint16_t servo_power_mv;
  int16_t servo_angle_ddeg;
  uint8_t flight_state;
  uint8_t error_code;
  uint8_t water_detected;

  TelemetryV2Fields();
};

uint16_t crc16CcittFalse(const uint8_t *data, size_t length);
uint8_t telemetryV1Xor(const uint8_t *data, size_t length);
bool isValidTelemetryV1(const uint8_t *frame, size_t length);
bool isValidTelemetryV2(const uint8_t *frame, size_t length);
bool encodeTelemetryV2(const TelemetryV2Fields &fields,
                       uint8_t *output,
                       size_t output_length);

}  // namespace protocol
}  // namespace rocket
