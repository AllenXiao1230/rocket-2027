#include <assert.h>
#include <unity.h>

#include "flight/actuator_controller.h"
#include "flight/flight_state_machine.h"
#include "flight/hardware_interlock.h"

using rocket::flight::ActuatorController;
using rocket::flight::DeploymentCriteria;
using rocket::flight::FlightInputs;
using rocket::flight::FlightState;
using rocket::flight::FlightStateMachine;
using rocket::flight::HardwareInterlock;
using rocket::flight::InterlockInputs;

namespace {

void testOpenLegacySafetyNeverPermitsActuation() {
  HardwareInterlock interlock;
  InterlockInputs inputs;
  inputs.software_armed = true;
  inputs.launch_committed = true;

  assert(!interlock.permitsActuation(inputs));
  assert((interlock.evaluate(inputs) &
          rocket::flight::kInhibitObserveOnlyBuild) != 0);
  assert((interlock.evaluate(inputs) &
          rocket::flight::kInhibitDedicatedArmCircuit) != 0);
}

void testActuatorAlwaysRemainsObserveOnly() {
  ActuatorController controller;
  DeploymentCriteria criteria;
  criteria.drogue = true;
  InterlockInputs inputs;
  inputs.software_armed = true;
  inputs.dedicated_arm_circuit_asserted = true;
  inputs.continuity_confirmed = true;
  inputs.energy_bus_ready = true;
  inputs.launch_committed = true;

  const rocket::flight::ActuatorStatus status =
      controller.update(criteria, inputs);
  assert(status.observe_only);
  assert(status.drogue_criteria_seen);
  assert(!status.main_criteria_seen);
  assert(!status.physical_output_applied);
  assert((status.inhibit_mask &
          rocket::flight::kInhibitObserveOnlyBuild) != 0);
}

void testEveryRuntimeInterlockTrueStillCannotClearObserveOnly() {
  InterlockInputs inputs;
  inputs.software_armed = true;
  inputs.dedicated_arm_circuit_asserted = true;
  inputs.continuity_confirmed = true;
  inputs.energy_bus_ready = true;
  inputs.launch_committed = true;
  HardwareInterlock interlock;
  assert(!interlock.permitsActuation(inputs));
  assert(interlock.evaluate(inputs) == rocket::flight::kInhibitObserveOnlyBuild);
}

void testNominalStateProgressionProducesCriteria() {
  FlightStateMachine machine;
  FlightInputs inputs;
  inputs.legacy_safety_high = true;
  inputs.altitude_valid = true;
  inputs.altitude_m = 100.0f;

  machine.update(inputs);
  assert(machine.state() == FlightState::kPreflight);

  inputs.acceleration_valid = true;
  inputs.acceleration_magnitude_g = 4.0f;
  inputs.now_ms = 1000;
  machine.update(inputs);
  inputs.now_ms = 1600;
  rocket::flight::FlightEvents events = machine.update(inputs);
  assert(events.liftoff_detected);
  assert(machine.state() == FlightState::kAscent);

  inputs.acceleration_magnitude_g = 1.0f;
  inputs.vertical_speed_valid = true;
  inputs.vertical_speed_mps = -2.0f;
  inputs.now_ms = 2000;
  machine.update(inputs);
  inputs.now_ms = 2600;
  events = machine.update(inputs);
  assert(events.apogee_detected);
  assert(machine.state() == FlightState::kApogee);

  inputs.altitude_m = 3200.0f;
  inputs.now_ms = 3200;
  machine.update(inputs);
  inputs.now_ms = 3800;
  events = machine.update(inputs);
  assert(events.drogue_criteria_satisfied);
  assert(machine.state() == FlightState::kDescent);

  inputs.altitude_m = 400.0f;
  inputs.vertical_speed_mps = -20.0f;
  inputs.now_ms = 4000;
  machine.update(inputs);
  inputs.now_ms = 4600;
  events = machine.update(inputs);
  assert(events.main_criteria_satisfied);
  assert(machine.mainRequested());
}

void testApogeeFallbackCannotRemainInAscentForever() {
  FlightStateMachine machine;
  FlightInputs inputs;
  inputs.legacy_safety_high = true;
  inputs.altitude_valid = true;
  machine.update(inputs);

  inputs.acceleration_valid = true;
  inputs.acceleration_magnitude_g = 4.0f;
  inputs.now_ms = 1000;
  machine.update(inputs);
  inputs.now_ms = 1600;
  machine.update(inputs);
  assert(machine.state() == FlightState::kAscent);

  inputs.acceleration_magnitude_g = 1.0f;
  inputs.vertical_speed_valid = false;
  inputs.now_ms = 31600;
  const rocket::flight::FlightEvents events = machine.update(inputs);
  assert(events.apogee_detected);
  assert(machine.state() == FlightState::kApogee);
}

void testLandingAndTerminalState() {
  FlightStateMachine machine;
  FlightInputs inputs;
  inputs.legacy_safety_high = true;
  inputs.altitude_valid = true;
  machine.update(inputs);
  inputs.acceleration_valid = true;
  inputs.acceleration_magnitude_g = 4.0f;
  inputs.now_ms = 100;
  machine.update(inputs);
  inputs.now_ms = 700;
  machine.update(inputs);
  inputs.acceleration_magnitude_g = 1.0f;
  inputs.vertical_speed_valid = true;
  inputs.vertical_speed_mps = -1.0f;
  inputs.now_ms = 800;
  machine.update(inputs);
  inputs.now_ms = 1400;
  machine.update(inputs);
  inputs.altitude_m = 3200.0f;
  inputs.now_ms = 2000;
  machine.update(inputs);
  inputs.now_ms = 2600;
  machine.update(inputs);
  assert(machine.state() == FlightState::kDescent);

  inputs.water_detected = true;
  inputs.now_ms = 3000;
  machine.update(inputs);
  inputs.now_ms = 3600;
  machine.update(inputs);
  assert(machine.state() == FlightState::kLanded);
  inputs.legacy_safety_high = false;
  machine.update(inputs);
  assert(machine.state() == FlightState::kLanded);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(testOpenLegacySafetyNeverPermitsActuation);
  RUN_TEST(testActuatorAlwaysRemainsObserveOnly);
  RUN_TEST(testEveryRuntimeInterlockTrueStillCannotClearObserveOnly);
  RUN_TEST(testNominalStateProgressionProducesCriteria);
  RUN_TEST(testApogeeFallbackCannotRemainInAscentForever);
  RUN_TEST(testLandingAndTerminalState);
  return UNITY_END();
}
