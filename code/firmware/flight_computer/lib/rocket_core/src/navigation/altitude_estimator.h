#pragma once

#include <stdint.h>

#include "model/navigation_types.h"

namespace rocket {
namespace navigation {

struct AltitudeEstimatorConfig {
  uint32_t minimum_velocity_interval_ms;
  uint32_t maximum_velocity_interval_ms;
  uint32_t stale_timeout_ms;
  float vertical_speed_filter_alpha;

  AltitudeEstimatorConfig()
      : minimum_velocity_interval_ms(10),
        maximum_velocity_interval_ms(1000),
        stale_timeout_ms(1000),
        vertical_speed_filter_alpha(0.2f) {}
};

class AltitudeEstimator {
 public:
  explicit AltitudeEstimator(
      const AltitudeEstimatorConfig &config = AltitudeEstimatorConfig());

  void reset();
  AltitudeEstimate update(const AltitudeInputs &inputs);
  const AltitudeEstimate &estimate() const;

 private:
  static uint32_t elapsed(uint32_t now_ms, uint32_t start_ms);
  static float clampFilterAlpha(float alpha);

  AltitudeEstimatorConfig config_;
  AltitudeEstimate estimate_;
  bool have_previous_altitude_;
  float previous_altitude_m_;
  uint32_t previous_altitude_ms_;
};

}  // namespace navigation
}  // namespace rocket
