#pragma once

#include <stdint.h>

#include "flight/hardware_interlock.h"

namespace rocket {
namespace flight {

struct DeploymentCriteria {
  bool drogue;
  bool main;

  DeploymentCriteria() : drogue(false), main(false) {}
};

struct ActuatorStatus {
  bool observe_only;
  bool drogue_criteria_seen;
  bool main_criteria_seen;
  bool physical_output_applied;
  uint16_t inhibit_mask;

  ActuatorStatus()
      : observe_only(true),
        drogue_criteria_seen(false),
        main_criteria_seen(false),
        physical_output_applied(false),
        inhibit_mask(kInhibitObserveOnlyBuild) {}
};

// Observe-only safety boundary between flight decisions and hardware.
//
// This class deliberately exposes no fire(), GPIO, PWM, servo-angle or backend
// registration API. Rocket 2027 has no verified dual-channel arm circuit,
// continuity input, energy-bus feedback, deployment pin map or actuator timing.
// Logical criteria are recorded for telemetry and tests while GPIO16/5/6 remain
// inactive. A physical implementation must be introduced later as a separately
// reviewed hardware-specific change with HIL evidence.
class ActuatorController {
 public:
  ActuatorController()
      : drogue_criteria_seen_(false),
        main_criteria_seen_(false) {}

  void forceSafe() {
    // There is intentionally no physical backend to call.
  }

  void reset() {
    forceSafe();
    drogue_criteria_seen_ = false;
    main_criteria_seen_ = false;
  }

  ActuatorStatus update(const DeploymentCriteria &criteria,
                        const InterlockInputs &interlock_inputs) {
    HardwareInterlock interlock;
    if (criteria.drogue) {
      drogue_criteria_seen_ = true;
    }
    if (criteria.main) {
      main_criteria_seen_ = true;
    }

    ActuatorStatus status;
    status.observe_only = true;
    status.drogue_criteria_seen = drogue_criteria_seen_;
    status.main_criteria_seen = main_criteria_seen_;
    status.physical_output_applied = false;
    status.inhibit_mask = interlock.evaluate(interlock_inputs);
    forceSafe();
    return status;
  }

 private:
  bool drogue_criteria_seen_;
  bool main_criteria_seen_;
};

}  // namespace flight
}  // namespace rocket
