#include "storage/sd_telemetry_store.h"

#include "storage/csv_formatter.h"

namespace rocket {
namespace storage {

SdTelemetryStore::SdTelemetryStore()
    : file_(nullptr), last_flush_ms_(0), healthy_(false) {}

bool SdTelemetryStore::attach(File &file) {
  file_ = &file;
  last_flush_ms_ = 0;
  healthy_ = static_cast<bool>(file);
  return healthy_;
}

bool SdTelemetryStore::writeHeaderIfEmpty() {
  if (!healthy_ || file_ == nullptr) {
    return false;
  }
  if (file_->size() != 0) {
    return true;
  }

  char output[384] = {};
  size_t written = 0;
  if (!formatTelemetryCsvHeader(output, sizeof(output), &written) ||
      file_->write(
          reinterpret_cast<const uint8_t *>(output), written) != written) {
    healthy_ = false;
  }
  return healthy_;
}

bool SdTelemetryStore::append(
    const model::TelemetryStorageRecord &record) {
  if (!healthy_ || file_ == nullptr) {
    return false;
  }

  char output[512] = {};
  size_t written = 0;
  if (!formatTelemetryCsvRow(record, output, sizeof(output), &written) ||
      file_->write(
          reinterpret_cast<const uint8_t *>(output), written) != written) {
    healthy_ = false;
  }
  return healthy_;
}

void SdTelemetryStore::service(
    uint32_t now_ms, uint32_t flush_interval_ms) {
  if (!healthy_ || file_ == nullptr) {
    return;
  }
  if (now_ms - last_flush_ms_ >= flush_interval_ms) {
    file_->flush();
    last_flush_ms_ = now_ms;
  }
}

bool SdTelemetryStore::healthy() const {
  return healthy_;
}

}  // namespace storage
}  // namespace rocket
