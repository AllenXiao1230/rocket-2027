#pragma once

#include <stdint.h>

namespace rocket {
namespace navigation {

enum class AltitudeSource : uint8_t {
  kNone = 0,
  kBarometer = 1,
  kGps = 2,
};

struct AltitudeInputs {
  uint32_t now_ms;
  bool barometer_valid;
  float barometer_altitude_m;
  bool gps_valid;
  float gps_altitude_m;

  AltitudeInputs()
      : now_ms(0),
        barometer_valid(false),
        barometer_altitude_m(0.0f),
        gps_valid(false),
        gps_altitude_m(0.0f) {}
};

struct AltitudeEstimate {
  bool altitude_valid;
  float altitude_m;
  AltitudeSource altitude_source;
  bool vertical_speed_valid;
  float vertical_speed_mps;

  AltitudeEstimate()
      : altitude_valid(false),
        altitude_m(0.0f),
        altitude_source(AltitudeSource::kNone),
        vertical_speed_valid(false),
        vertical_speed_mps(0.0f) {}
};

}  // namespace navigation
}  // namespace rocket
