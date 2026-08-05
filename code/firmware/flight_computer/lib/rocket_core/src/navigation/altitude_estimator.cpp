#include "navigation/altitude_estimator.h"

namespace rocket {
namespace navigation {

AltitudeEstimator::AltitudeEstimator(const AltitudeEstimatorConfig &config)
    : config_(config) {
  config_.vertical_speed_filter_alpha =
      clampFilterAlpha(config_.vertical_speed_filter_alpha);
  reset();
}

void AltitudeEstimator::reset() {
  estimate_ = AltitudeEstimate();
  have_previous_altitude_ = false;
  previous_altitude_m_ = 0.0f;
  previous_altitude_ms_ = 0;
}

AltitudeEstimate AltitudeEstimator::update(const AltitudeInputs &inputs) {
  estimate_.altitude_valid = false;
  estimate_.altitude_m = 0.0f;
  estimate_.altitude_source = AltitudeSource::kNone;

  if (inputs.barometer_valid) {
    estimate_.altitude_valid = true;
    estimate_.altitude_m = inputs.barometer_altitude_m;
    estimate_.altitude_source = AltitudeSource::kBarometer;
  } else if (inputs.gps_valid) {
    estimate_.altitude_valid = true;
    estimate_.altitude_m = inputs.gps_altitude_m;
    estimate_.altitude_source = AltitudeSource::kGps;
  }

  if (estimate_.altitude_valid) {
    if (have_previous_altitude_) {
      const uint32_t delta_ms =
          elapsed(inputs.now_ms, previous_altitude_ms_);
      if (delta_ms > config_.minimum_velocity_interval_ms &&
          delta_ms < config_.maximum_velocity_interval_ms) {
        const float instantaneous_vertical_speed_mps =
            (estimate_.altitude_m - previous_altitude_m_) *
            (1000.0f / static_cast<float>(delta_ms));
        if (!estimate_.vertical_speed_valid) {
          estimate_.vertical_speed_mps =
              instantaneous_vertical_speed_mps;
        } else {
          estimate_.vertical_speed_mps +=
              config_.vertical_speed_filter_alpha *
              (instantaneous_vertical_speed_mps -
               estimate_.vertical_speed_mps);
        }
        estimate_.vertical_speed_valid = true;
      }
    }

    have_previous_altitude_ = true;
    previous_altitude_m_ = estimate_.altitude_m;
    previous_altitude_ms_ = inputs.now_ms;
  } else if (have_previous_altitude_ &&
             elapsed(inputs.now_ms, previous_altitude_ms_) >
                 config_.stale_timeout_ms) {
    estimate_.vertical_speed_valid = false;
  }

  return estimate_;
}

const AltitudeEstimate &AltitudeEstimator::estimate() const {
  return estimate_;
}

uint32_t AltitudeEstimator::elapsed(uint32_t now_ms, uint32_t start_ms) {
  return now_ms - start_ms;
}

float AltitudeEstimator::clampFilterAlpha(float alpha) {
  if (alpha < 0.0f) {
    return 0.0f;
  }
  if (alpha > 1.0f) {
    return 1.0f;
  }
  return alpha;
}

}  // namespace navigation
}  // namespace rocket
