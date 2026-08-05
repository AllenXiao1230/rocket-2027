#pragma once

#include <stdint.h>

namespace rocket {
namespace flight {

enum InterlockInhibit : uint16_t {
  kInhibitNone = 0,
  kInhibitObserveOnlyBuild = 1u << 0,
  kInhibitSoftwareNotArmed = 1u << 1,
  kInhibitDedicatedArmCircuit = 1u << 2,
  kInhibitContinuityUnknown = 1u << 3,
  kInhibitEnergyBusNotReady = 1u << 4,
  kInhibitLaunchNotCommitted = 1u << 5,
  kInhibitFatalFault = 1u << 6,
};

struct InterlockInputs {
  bool software_armed;
  bool dedicated_arm_circuit_asserted;
  bool continuity_confirmed;
  bool energy_bus_ready;
  bool launch_committed;
  bool fatal_fault;

  InterlockInputs()
      : software_armed(false),
        dedicated_arm_circuit_asserted(false),
        continuity_confirmed(false),
        energy_bus_ready(false),
        launch_committed(false),
        fatal_fault(false) {}
};

class HardwareInterlock {
 public:
  uint16_t evaluate(const InterlockInputs &inputs) const {
    // Rocket 2027 is deliberately compiled as ObserveOnly. This inhibit
    // cannot be cleared by runtime data or a caller-controlled boolean.
    uint16_t inhibit = kInhibitObserveOnlyBuild;
    if (!inputs.software_armed) {
      inhibit |= kInhibitSoftwareNotArmed;
    }
    if (!inputs.dedicated_arm_circuit_asserted) {
      inhibit |= kInhibitDedicatedArmCircuit;
    }
    if (!inputs.continuity_confirmed) {
      inhibit |= kInhibitContinuityUnknown;
    }
    if (!inputs.energy_bus_ready) {
      inhibit |= kInhibitEnergyBusNotReady;
    }
    if (!inputs.launch_committed) {
      inhibit |= kInhibitLaunchNotCommitted;
    }
    if (inputs.fatal_fault) {
      inhibit |= kInhibitFatalFault;
    }
    return inhibit;
  }

  bool permitsActuation(const InterlockInputs &inputs) const {
    return evaluate(inputs) == kInhibitNone;
  }
};

}  // namespace flight
}  // namespace rocket
