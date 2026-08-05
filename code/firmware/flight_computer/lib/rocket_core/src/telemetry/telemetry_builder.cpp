#include "telemetry/telemetry_builder.h"

#include <cmath>
#include <limits>

namespace rocket {
namespace telemetry {
namespace {

bool finite(double value) {
  return std::isfinite(value);
}

int16_t scaledI16(double value, double scale) {
  const double scaled = value * scale;
  const double minimum =
      static_cast<double>(std::numeric_limits<int16_t>::min()) + 1.0;
  const double maximum =
      static_cast<double>(std::numeric_limits<int16_t>::max());
  if (scaled <= minimum) {
    return static_cast<int16_t>(minimum);
  }
  if (scaled >= maximum) {
    return static_cast<int16_t>(maximum);
  }
  return static_cast<int16_t>(std::llround(scaled));
}

int32_t scaledI32(double value, double scale) {
  const double scaled = value * scale;
  const double minimum =
      static_cast<double>(std::numeric_limits<int32_t>::min()) + 1.0;
  const double maximum =
      static_cast<double>(std::numeric_limits<int32_t>::max());
  if (scaled <= minimum) {
    return static_cast<int32_t>(minimum);
  }
  if (scaled >= maximum) {
    return static_cast<int32_t>(maximum);
  }
  return static_cast<int32_t>(std::llround(scaled));
}

uint16_t scaledU16(double value, double scale) {
  const double scaled = value * scale;
  const double maximum =
      static_cast<double>(std::numeric_limits<uint16_t>::max()) - 1.0;
  if (scaled <= 0.0) {
    return 0;
  }
  if (scaled >= maximum) {
    return static_cast<uint16_t>(maximum);
  }
  return static_cast<uint16_t>(std::llround(scaled));
}

uint8_t clampedU8(uint16_t value) {
  const uint16_t maximum =
      static_cast<uint16_t>(std::numeric_limits<uint8_t>::max()) - 1U;
  return static_cast<uint8_t>(value > maximum ? maximum : value);
}

uint16_t normalizedHeadingDdeg(double heading_deg) {
  double normalized = std::fmod(heading_deg, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }
  const uint16_t rounded =
      static_cast<uint16_t>(std::llround(normalized * 10.0));
  return static_cast<uint16_t>(rounded % 3600U);
}

bool validPosition(const model::GpsTelemetrySample &gps) {
  return gps.position_valid &&
         finite(gps.latitude_deg) &&
         finite(gps.longitude_deg) &&
         gps.latitude_deg >= -90.0 &&
         gps.latitude_deg <= 90.0 &&
         gps.longitude_deg >= -180.0 &&
         gps.longitude_deg <= 180.0;
}

bool validHeading(const model::NavigationTelemetrySample &navigation) {
  if (!navigation.heading_valid || !finite(navigation.heading_deg)) {
    return false;
  }
  return navigation.heading_source == model::HeadingTelemetrySource::kImu ||
         navigation.heading_source == model::HeadingTelemetrySource::kGps;
}

}  // namespace

protocol::TelemetryV2Fields TelemetryBuilder::build(
    const model::TelemetrySample &sample) const {
  protocol::TelemetryV2Fields fields;
  fields.source_flags = 0;
  fields.boot_id = sample.context.boot_id;
  fields.sequence = sample.context.sequence;
  fields.uptime_ms = sample.context.uptime_ms;
  fields.valid_flags = 0;

  if (validPosition(sample.gps)) {
    fields.valid_flags |= protocol::kValidGpsPosition;
    fields.latitude_e7 =
        scaledI32(sample.gps.latitude_deg, 10000000.0);
    fields.longitude_e7 =
        scaledI32(sample.gps.longitude_deg, 10000000.0);
  }

  if (sample.gps.altitude_valid && finite(sample.gps.altitude_m)) {
    fields.valid_flags |= protocol::kValidGpsAltitude;
    fields.gps_altitude_dm =
        scaledI16(sample.gps.altitude_m, 10.0);
  }

  if (sample.gps.speed_valid &&
      finite(sample.gps.speed_mps) &&
      sample.gps.speed_mps >= 0.0f) {
    fields.valid_flags |= protocol::kValidGpsSpeed;
    fields.gps_speed_dms =
        scaledI16(sample.gps.speed_mps, 10.0);
  }

  if (sample.gps.satellite_count_valid) {
    fields.valid_flags |= protocol::kValidSatelliteCount;
    fields.satellite_count =
        clampedU8(sample.gps.satellite_count);
  }

  if (sample.navigation.attitude_valid &&
      finite(sample.navigation.roll_deg) &&
      finite(sample.navigation.pitch_deg)) {
    fields.valid_flags |= protocol::kValidAttitude;
    fields.roll_cdeg =
        scaledI16(sample.navigation.roll_deg, 100.0);
    fields.pitch_cdeg =
        scaledI16(sample.navigation.pitch_deg, 100.0);
  }

  if (validHeading(sample.navigation)) {
    fields.valid_flags |= protocol::kValidHeading;
    fields.heading_ddeg =
        normalizedHeadingDdeg(sample.navigation.heading_deg);
    if (sample.navigation.heading_source ==
        model::HeadingTelemetrySource::kImu) {
      fields.source_flags |= protocol::kSourceHeadingImu;
    } else {
      fields.source_flags |= protocol::kSourceHeadingGps;
    }
  }

  if (sample.motion.gyro_valid &&
      finite(sample.motion.gyro_x_dps) &&
      finite(sample.motion.gyro_y_dps) &&
      finite(sample.motion.gyro_z_dps)) {
    fields.valid_flags |= protocol::kValidGyro;
    fields.gyro_x_ddeg_s =
        scaledI16(sample.motion.gyro_x_dps, 10.0);
    fields.gyro_y_ddeg_s =
        scaledI16(sample.motion.gyro_y_dps, 10.0);
    fields.gyro_z_ddeg_s =
        scaledI16(sample.motion.gyro_z_dps, 10.0);
  }

  if (sample.motion.acceleration_valid &&
      finite(sample.motion.acceleration_x_g) &&
      finite(sample.motion.acceleration_y_g) &&
      finite(sample.motion.acceleration_z_g)) {
    fields.valid_flags |= protocol::kValidAcceleration;
    fields.accel_x_cg =
        scaledI16(sample.motion.acceleration_x_g, 100.0);
    fields.accel_y_cg =
        scaledI16(sample.motion.acceleration_y_g, 100.0);
    fields.accel_z_cg =
        scaledI16(sample.motion.acceleration_z_g, 100.0);
  }

  if (sample.environment.barometric_altitude_valid &&
      finite(sample.environment.barometric_altitude_m)) {
    fields.valid_flags |= protocol::kValidBaroAltitude;
    fields.baro_altitude_dm =
        scaledI16(sample.environment.barometric_altitude_m, 10.0);
  }

  if (sample.power.battery_valid &&
      finite(sample.power.battery_v) &&
      sample.power.battery_v >= 0.0f) {
    fields.valid_flags |= protocol::kValidBattery;
    fields.battery_mv =
        scaledU16(sample.power.battery_v, 1000.0);
  }

  if (sample.power.servo_power_valid &&
      finite(sample.power.servo_power_v) &&
      sample.power.servo_power_v >= 0.0f) {
    fields.valid_flags |= protocol::kValidServoPower;
    fields.servo_power_mv =
        scaledU16(sample.power.servo_power_v, 1000.0);
  }

  if (sample.power.servo_angle_valid &&
      finite(sample.power.servo_angle_deg)) {
    fields.valid_flags |= protocol::kValidServoAngle;
    fields.servo_angle_ddeg =
        scaledI16(sample.power.servo_angle_deg, 10.0);
  }

  if (sample.flight.state_valid &&
      sample.flight.flight_state != protocol::kInvalidU8) {
    fields.valid_flags |= protocol::kValidFlightState;
    fields.flight_state = sample.flight.flight_state;
  }

  if (sample.flight.error_code_valid) {
    fields.valid_flags |= protocol::kValidErrorCode;
    fields.error_code = sample.flight.error_code;
  }

  if (sample.flight.water_valid) {
    fields.valid_flags |= protocol::kValidWater;
    fields.water_detected =
        sample.flight.water_detected ? 1U : 0U;
  }

  if (sample.navigation.state_altitude_valid) {
    if (sample.navigation.state_altitude_source ==
            model::StateAltitudeTelemetrySource::kBarometer &&
        (fields.valid_flags & protocol::kValidBaroAltitude) != 0) {
      fields.source_flags |= protocol::kSourceStateAltitudeBaro;
    } else if (sample.navigation.state_altitude_source ==
                   model::StateAltitudeTelemetrySource::kGps &&
               (fields.valid_flags &
                protocol::kValidGpsAltitude) != 0) {
      fields.source_flags |= protocol::kSourceStateAltitudeGps;
    }
  }

  return fields;
}

}  // namespace telemetry
}  // namespace rocket
