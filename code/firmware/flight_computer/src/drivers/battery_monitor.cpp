#include "drivers/battery_monitor.h"

#include <Arduino.h>

namespace rocket {
namespace drivers {

BatteryMonitor::BatteryMonitor(uint8_t adc_pin, float resistor_top_ohm,
                               float resistor_bottom_ohm)
    : adc_pin_(adc_pin),
      resistor_top_ohm_(resistor_top_ohm),
      resistor_bottom_ohm_(resistor_bottom_ohm) {}

void BatteryMonitor::begin() const {
  analogReadResolution(12);
  analogSetPinAttenuation(adc_pin_, ADC_11db);
}

uint16_t BatteryMonitor::readMillivolts() const {
  return scaleAdcMillivolts(
      analogReadMilliVolts(adc_pin_),
      resistor_top_ohm_,
      resistor_bottom_ohm_);
}

}  // namespace drivers
}  // namespace rocket

