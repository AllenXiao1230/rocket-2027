#pragma once

#include <stdint.h>

namespace rocket {
namespace protocol {

constexpr uint8_t kTelemetryV2Sync0 = 0xA5;
constexpr uint8_t kTelemetryV2Sync1 = 0x5A;
constexpr uint8_t kTelemetryV2Version = 2;
constexpr uint8_t kTelemetryV2FrameLength = 64;
constexpr uint8_t kTelemetryV2MessageType = 1;
constexpr uint8_t kTelemetryV1FrameLength = 47;

enum TelemetryValidFlag : uint32_t {
  kValidGpsPosition = 1UL << 0,
  kValidGpsAltitude = 1UL << 1,
  kValidGpsSpeed = 1UL << 2,
  kValidSatelliteCount = 1UL << 3,
  kValidAttitude = 1UL << 4,
  kValidHeading = 1UL << 5,
  kValidGyro = 1UL << 6,
  kValidAcceleration = 1UL << 7,
  kValidBaroAltitude = 1UL << 8,
  kValidBattery = 1UL << 9,
  kValidServoPower = 1UL << 10,
  kValidServoAngle = 1UL << 11,
  kValidFlightState = 1UL << 12,
  kValidErrorCode = 1UL << 13,
  kValidWater = 1UL << 14,
};

enum TelemetrySourceFlag : uint8_t {
  kSourceHeadingImu = 1U << 0,
  kSourceHeadingGps = 1U << 1,
  kSourceStateAltitudeBaro = 1U << 2,
  kSourceStateAltitudeGps = 1U << 3,
};

enum TelemetryV2Offset : uint8_t {
  kOffsetSync = 0,
  kOffsetVersion = 2,
  kOffsetTotalLength = 3,
  kOffsetMessageType = 4,
  kOffsetSourceFlags = 5,
  kOffsetBootId = 6,
  kOffsetSequence = 8,
  kOffsetUptimeMs = 12,
  kOffsetValidFlags = 16,
  kOffsetLatitudeE7 = 20,
  kOffsetLongitudeE7 = 24,
  kOffsetGpsAltitudeDm = 28,
  kOffsetGpsSpeedDms = 30,
  kOffsetSatelliteCount = 32,
  kOffsetRollCdeg = 33,
  kOffsetPitchCdeg = 35,
  kOffsetHeadingDdeg = 37,
  kOffsetGyroXDdegS = 39,
  kOffsetGyroYDdegS = 41,
  kOffsetGyroZDdegS = 43,
  kOffsetAccelXCg = 45,
  kOffsetAccelYCg = 47,
  kOffsetAccelZCg = 49,
  kOffsetBaroAltitudeDm = 51,
  kOffsetBatteryMv = 53,
  kOffsetServoPowerMv = 55,
  kOffsetServoAngleDdeg = 57,
  kOffsetFlightState = 59,
  kOffsetErrorCode = 60,
  kOffsetWaterDetected = 61,
  kOffsetCrc16 = 62,
};

constexpr uint8_t kCrcCoveredLength = 62;
constexpr int32_t kInvalidI32 = INT32_MIN;
constexpr int16_t kInvalidI16 = INT16_MIN;
constexpr uint16_t kInvalidU16 = UINT16_MAX;
constexpr uint8_t kInvalidU8 = UINT8_MAX;

}  // namespace protocol
}  // namespace rocket
