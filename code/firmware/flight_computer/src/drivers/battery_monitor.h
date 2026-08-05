#pragma once

#include <math.h>
#include <stdint.h>

namespace rocket {
namespace drivers {

class BatteryMonitor {
 public:
  BatteryMonitor(uint8_t adc_pin, float resistor_top_ohm,
                 float resistor_bottom_ohm);

  void begin() const;
  uint16_t readMillivolts() const;

  static uint16_t scaleAdcMillivolts(uint32_t adc_mv,
                                    float resistor_top_ohm,
                                    float resistor_bottom_ohm) {
    if (adc_mv == 0 ||
        resistor_top_ohm < 0.0f ||
        resistor_bottom_ohm <= 0.0f) {
      return 0;
    }
    const float divider =
        resistor_bottom_ohm / (resistor_top_ohm + resistor_bottom_ohm);
    const long battery_mv =
        lroundf(static_cast<float>(adc_mv) / divider);
    if (battery_mv <= 0) {
      return 0;
    }
    if (battery_mv >= 65535L) {
      return UINT16_MAX;
    }
    return static_cast<uint16_t>(battery_mv);
  }

 private:
  uint8_t adc_pin_;
  float resistor_top_ohm_;
  float resistor_bottom_ohm_;
};

}  // namespace drivers
}  // namespace rocket

