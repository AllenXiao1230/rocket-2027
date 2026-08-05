#pragma once

#include <FS.h>
#include <stdint.h>

#include "model/telemetry_storage_record.h"

namespace rocket {
namespace storage {

// Arduino/SD adapter around the hardware-independent CSV formatter.
class SdTelemetryStore {
 public:
  SdTelemetryStore();

  bool attach(File &file);
  bool writeHeaderIfEmpty();
  bool append(const model::TelemetryStorageRecord &record);
  void service(uint32_t now_ms, uint32_t flush_interval_ms = 1000);
  bool healthy() const;

 private:
  File *file_;
  uint32_t last_flush_ms_;
  bool healthy_;
};

}  // namespace storage
}  // namespace rocket

