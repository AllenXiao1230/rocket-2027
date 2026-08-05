#pragma once

#include <TelemetryProtocol.h>

#include "model/telemetry_sample.h"

namespace rocket {
namespace telemetry {

class TelemetryBuilder {
 public:
  protocol::TelemetryV2Fields build(
      const model::TelemetrySample &sample) const;
};

}  // namespace telemetry
}  // namespace rocket
