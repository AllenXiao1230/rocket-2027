#pragma once

#include <stddef.h>

#include "model/telemetry_storage_record.h"

namespace rocket {
namespace storage {

constexpr size_t kTelemetryCsvColumnCount = 26;

extern const char kTelemetryCsvHeader[];

// Formats the fixed telemetry CSV header, including its trailing newline.
//
// On success, returns true and stores the byte count excluding the terminating
// NUL in written when written is non-null. On any failure, returns false,
// stores zero in written, and makes output an empty string when possible.
bool formatTelemetryCsvHeader(char *output,
                              size_t output_capacity,
                              size_t *written);

// Formats one fixed-column telemetry row, including its trailing newline.
// Invalid or non-finite values are emitted as empty fields.
//
// The function never reports or exposes a truncated row. The output buffer is
// an empty string on failure when output_capacity is non-zero.
bool formatTelemetryCsvRow(const model::TelemetryStorageRecord &record,
                           char *output,
                           size_t output_capacity,
                           size_t *written);

}  // namespace storage
}  // namespace rocket
