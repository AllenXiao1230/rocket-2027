#pragma once

#include <stdint.h>
#include <math.h>

namespace rocket {
namespace flight {

enum class FlightState : uint8_t {
  kTest = 0,
  kIdle = 1,
  kPreflight = 2,
  kAscent = 3,
  kApogee = 4,
  kDescent = 5,
  kLanded = 6,
  kAbort = 99,
};

struct FlightThresholds {
  float liftoff_accel_g;
  float liftoff_vertical_speed_mps;
  float apogee_vertical_speed_mps;
  float drogue_min_relative_altitude_m;
  float drogue_max_vertical_speed_mps;
  float main_max_relative_altitude_m;
  float main_max_vertical_speed_mps;
  float landing_max_relative_altitude_m;
  float landing_max_vertical_speed_mps;
  uint32_t liftoff_hold_ms;
  uint32_t apogee_hold_ms;
  uint32_t drogue_hold_ms;
  uint32_t main_hold_ms;
  uint32_t landing_hold_ms;
  uint32_t after_apogee_ms;
  uint32_t apogee_fallback_ms;
  uint32_t drogue_fallback_ms;
  uint32_t main_fallback_ms;

  FlightThresholds()
      : liftoff_accel_g(3.0f),
        liftoff_vertical_speed_mps(10.0f),
        apogee_vertical_speed_mps(-0.5f),
        drogue_min_relative_altitude_m(3000.0f),
        drogue_max_vertical_speed_mps(5.0f),
        main_max_relative_altitude_m(500.0f),
        main_max_vertical_speed_mps(5.0f),
        landing_max_relative_altitude_m(30.0f),
        landing_max_vertical_speed_mps(5.0f),
        liftoff_hold_ms(500),
        apogee_hold_ms(500),
        drogue_hold_ms(500),
        main_hold_ms(500),
        landing_hold_ms(500),
        after_apogee_ms(500),
        apogee_fallback_ms(30000),
        drogue_fallback_ms(25000),
        main_fallback_ms(190000) {}
};

struct FlightInputs {
  uint32_t now_ms;
  bool legacy_safety_high;
  bool acceleration_valid;
  float acceleration_magnitude_g;
  bool altitude_valid;
  float altitude_m;
  bool vertical_speed_valid;
  float vertical_speed_mps;
  bool water_detected;

  FlightInputs()
      : now_ms(0),
        legacy_safety_high(false),
        acceleration_valid(false),
        acceleration_magnitude_g(0.0f),
        altitude_valid(false),
        altitude_m(0.0f),
        vertical_speed_valid(false),
        vertical_speed_mps(0.0f),
        water_detected(false) {}
};

struct FlightEvents {
  bool state_changed;
  bool liftoff_detected;
  bool apogee_detected;
  bool drogue_criteria_satisfied;
  bool main_criteria_satisfied;
  bool landed_detected;

  FlightEvents()
      : state_changed(false),
        liftoff_detected(false),
        apogee_detected(false),
        drogue_criteria_satisfied(false),
        main_criteria_satisfied(false),
        landed_detected(false) {}
};

class FlightStateMachine {
 public:
  explicit FlightStateMachine(
      const FlightThresholds &thresholds = FlightThresholds())
      : thresholds_(thresholds) {
    reset();
  }

  void reset() {
    state_ = FlightState::kIdle;
    previous_state_ = FlightState::kIdle;
    liftoff_ms_ = 0;
    apogee_ms_ = 0;
    altitude_zero_m_ = 0.0f;
    altitude_zero_valid_ = false;
    main_requested_ = false;
    liftoff_condition_ms_ = 0;
    apogee_condition_ms_ = 0;
    drogue_condition_ms_ = 0;
    main_condition_ms_ = 0;
    landing_condition_ms_ = 0;
  }

  FlightEvents update(const FlightInputs &inputs) {
    FlightEvents events;
    previous_state_ = state_;

    switch (state_) {
      case FlightState::kIdle:
        resetFlightTracking();
        if (inputs.legacy_safety_high) {
          state_ = FlightState::kPreflight;
        }
        break;

      case FlightState::kPreflight:
        if (!inputs.legacy_safety_high) {
          state_ = FlightState::kIdle;
          break;
        }
        if (!altitude_zero_valid_ && inputs.altitude_valid) {
          altitude_zero_m_ = inputs.altitude_m;
          altitude_zero_valid_ = true;
        }
        if (held(
                (inputs.acceleration_valid &&
                 inputs.acceleration_magnitude_g >
                     thresholds_.liftoff_accel_g) ||
                    (inputs.vertical_speed_valid &&
                     inputs.vertical_speed_mps >
                         thresholds_.liftoff_vertical_speed_mps),
                liftoff_condition_ms_,
                inputs.now_ms,
                thresholds_.liftoff_hold_ms)) {
          liftoff_ms_ = inputs.now_ms;
          apogee_ms_ = 0;
          main_requested_ = false;
          clearConditionTimersAfterLiftoff();
          state_ = FlightState::kAscent;
          events.liftoff_detected = true;
        }
        break;

      case FlightState::kAscent: {
        const bool vertical_apogee =
            inputs.vertical_speed_valid &&
            inputs.vertical_speed_mps <
                thresholds_.apogee_vertical_speed_mps;
        const bool timed_fallback =
            liftoff_ms_ != 0 &&
            elapsed(inputs.now_ms, liftoff_ms_) >=
                thresholds_.apogee_fallback_ms;
        if (timed_fallback ||
            held(vertical_apogee,
                 apogee_condition_ms_,
                 inputs.now_ms,
                 thresholds_.apogee_hold_ms)) {
          apogee_ms_ = inputs.now_ms;
          drogue_condition_ms_ = 0;
          state_ = FlightState::kApogee;
          events.apogee_detected = true;
        }
        break;
      }

      case FlightState::kApogee: {
        const bool after_apogee =
            apogee_ms_ != 0 &&
            elapsed(inputs.now_ms, apogee_ms_) >=
                thresholds_.after_apogee_ms;
        const bool vertical_speed_safe =
            !inputs.vertical_speed_valid ||
            inputs.vertical_speed_mps <=
                thresholds_.drogue_max_vertical_speed_mps;
        const bool altitude_condition =
            inputs.altitude_valid &&
            relativeAltitude(inputs.altitude_m) >
                thresholds_.drogue_min_relative_altitude_m &&
            vertical_speed_safe;
        const bool timed_fallback =
            liftoff_ms_ != 0 &&
            elapsed(inputs.now_ms, liftoff_ms_) >=
                thresholds_.drogue_fallback_ms;
        if (after_apogee &&
            held(altitude_condition || timed_fallback,
                 drogue_condition_ms_,
                 inputs.now_ms,
                 thresholds_.drogue_hold_ms)) {
          state_ = FlightState::kDescent;
          main_condition_ms_ = 0;
          landing_condition_ms_ = 0;
          events.drogue_criteria_satisfied = true;
        }
        break;
      }

      case FlightState::kDescent: {
        const bool altitude_main_condition =
            inputs.altitude_valid &&
            inputs.vertical_speed_valid &&
            relativeAltitude(inputs.altitude_m) <
                thresholds_.main_max_relative_altitude_m &&
            inputs.vertical_speed_mps <
                thresholds_.main_max_vertical_speed_mps;
        const bool timed_main_fallback =
            liftoff_ms_ != 0 &&
            elapsed(inputs.now_ms, liftoff_ms_) >=
                thresholds_.main_fallback_ms;
        if (!main_requested_ &&
            held(altitude_main_condition || timed_main_fallback,
                 main_condition_ms_,
                 inputs.now_ms,
                 thresholds_.main_hold_ms)) {
          main_requested_ = true;
          events.main_criteria_satisfied = true;
        }

        const bool landing_altitude =
            inputs.altitude_valid &&
            relativeAltitude(inputs.altitude_m) <
                thresholds_.landing_max_relative_altitude_m;
        const bool landing_speed =
            !inputs.vertical_speed_valid ||
            fabsf(inputs.vertical_speed_mps) <
                thresholds_.landing_max_vertical_speed_mps;
        if (held(inputs.water_detected ||
                     (landing_altitude && landing_speed),
                 landing_condition_ms_,
                 inputs.now_ms,
                 thresholds_.landing_hold_ms)) {
          state_ = FlightState::kLanded;
          events.landed_detected = true;
        }
        break;
      }

      case FlightState::kLanded:
      case FlightState::kAbort:
      case FlightState::kTest:
      default:
        break;
    }

    events.state_changed = state_ != previous_state_;
    return events;
  }

  void abort() {
    previous_state_ = state_;
    state_ = FlightState::kAbort;
  }

  FlightState state() const { return state_; }
  uint8_t stateCode() const {
    return static_cast<uint8_t>(state_);
  }
  uint32_t liftoffMs() const { return liftoff_ms_; }
  bool mainRequested() const { return main_requested_; }
  bool altitudeZeroValid() const { return altitude_zero_valid_; }
  float altitudeZeroM() const { return altitude_zero_m_; }

  float flightTimeSeconds(uint32_t now_ms) const {
    return liftoff_ms_ == 0
               ? 0.0f
               : static_cast<float>(elapsed(now_ms, liftoff_ms_)) /
                     1000.0f;
  }

 private:
  static uint32_t elapsed(uint32_t now_ms, uint32_t start_ms) {
    return now_ms - start_ms;
  }

  static bool held(bool condition,
                   uint32_t &condition_start_ms,
                   uint32_t now_ms,
                   uint32_t hold_ms) {
    if (!condition) {
      condition_start_ms = 0;
      return false;
    }
    if (condition_start_ms == 0) {
      condition_start_ms = now_ms == 0 ? 1 : now_ms;
    }
    return elapsed(now_ms, condition_start_ms) >= hold_ms;
  }

  float relativeAltitude(float altitude_m) const {
    return altitude_zero_valid_
               ? altitude_m - altitude_zero_m_
               : altitude_m;
  }

  void resetFlightTracking() {
    liftoff_ms_ = 0;
    apogee_ms_ = 0;
    altitude_zero_m_ = 0.0f;
    altitude_zero_valid_ = false;
    main_requested_ = false;
    liftoff_condition_ms_ = 0;
    apogee_condition_ms_ = 0;
    drogue_condition_ms_ = 0;
    main_condition_ms_ = 0;
    landing_condition_ms_ = 0;
  }

  void clearConditionTimersAfterLiftoff() {
    liftoff_condition_ms_ = 0;
    apogee_condition_ms_ = 0;
    drogue_condition_ms_ = 0;
    main_condition_ms_ = 0;
    landing_condition_ms_ = 0;
  }

  FlightThresholds thresholds_;
  FlightState state_;
  FlightState previous_state_;
  uint32_t liftoff_ms_;
  uint32_t apogee_ms_;
  float altitude_zero_m_;
  bool altitude_zero_valid_;
  bool main_requested_;
  uint32_t liftoff_condition_ms_;
  uint32_t apogee_condition_ms_;
  uint32_t drogue_condition_ms_;
  uint32_t main_condition_ms_;
  uint32_t landing_condition_ms_;
};

}  // namespace flight
}  // namespace rocket
