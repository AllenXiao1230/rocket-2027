#include "storage/csv_formatter.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

namespace rocket {
namespace storage {

const char kTelemetryCsvHeader[] =
    "time_ms,rtc,lat,lon,gps_alt,baro_alt,speed,heading,sat,roll,pitch,"
    "accx,accy,accz,gyro_x,gyro_y,gyro_z,battery,servo_power_mv,"
    "servo_angle_deg,temp,hum,pressure_pa,status,error,water";

namespace {

class CsvWriter {
 public:
  CsvWriter(char *output, size_t capacity)
      : output_(output),
        capacity_(capacity),
        length_(0),
        valid_(output != nullptr && capacity > 0) {
    if (valid_) {
      output_[0] = '\0';
    }
  }

  bool append(const char *format, ...) {
    if (!valid_ || format == nullptr) {
      invalidate();
      return false;
    }

    const size_t remaining = capacity_ - length_;
    va_list args;
    va_start(args, format);
    const int result =
        vsnprintf(output_ + length_, remaining, format, args);
    va_end(args);

    // C99 snprintf returns the number of bytes that would have been written.
    // Equality therefore means the terminating NUL did not fit.
    if (result < 0 || static_cast<size_t>(result) >= remaining) {
      invalidate();
      return false;
    }

    length_ += static_cast<size_t>(result);
    return true;
  }

  bool finish(size_t *written) const {
    if (written != nullptr) {
      *written = valid_ ? length_ : 0;
    }
    return valid_;
  }

 private:
  void invalidate() {
    valid_ = false;
    length_ = 0;
    if (output_ != nullptr && capacity_ > 0) {
      output_[0] = '\0';
    }
  }

  char *output_;
  size_t capacity_;
  size_t length_;
  bool valid_;
};

template <typename T>
bool appendUnsigned(CsvWriter &writer, const model::ValidValue<T> &field) {
  if (!writer.append(",")) {
    return false;
  }
  if (!field.valid) {
    return true;
  }
  return writer.append("%lu", static_cast<unsigned long>(field.value));
}

template <typename T>
bool appendFloat(CsvWriter &writer,
                 const model::ValidValue<T> &field,
                 int precision) {
  if (!writer.append(",")) {
    return false;
  }
  if (!field.valid) {
    return true;
  }
  const double value = static_cast<double>(field.value);
  if (!isfinite(value)) {
    return true;
  }
  return writer.append("%.*f", precision, value);
}

void clearWritten(size_t *written) {
  if (written != nullptr) {
    *written = 0;
  }
}

}  // namespace

bool formatTelemetryCsvHeader(char *output,
                              size_t output_capacity,
                              size_t *written) {
  clearWritten(written);
  CsvWriter writer(output, output_capacity);
  if (!writer.append("%s\n", kTelemetryCsvHeader)) {
    return writer.finish(written);
  }
  return writer.finish(written);
}

bool formatTelemetryCsvRow(const model::TelemetryStorageRecord &record,
                           char *output,
                           size_t output_capacity,
                           size_t *written) {
  clearWritten(written);
  CsvWriter writer(output, output_capacity);

  const bool formatted =
      writer.append("%lu", static_cast<unsigned long>(record.time_ms)) &&
      appendUnsigned(writer, record.rtc_unix) &&
      appendFloat(writer, record.latitude_deg, 7) &&
      appendFloat(writer, record.longitude_deg, 7) &&
      appendFloat(writer, record.gps_altitude_m, 1) &&
      appendFloat(writer, record.baro_altitude_m, 1) &&
      appendFloat(writer, record.speed_mps, 1) &&
      appendFloat(writer, record.heading_deg, 1) &&
      appendUnsigned(writer, record.satellite_count) &&
      appendFloat(writer, record.roll_deg, 2) &&
      appendFloat(writer, record.pitch_deg, 2) &&
      appendFloat(writer, record.accel_x_g, 2) &&
      appendFloat(writer, record.accel_y_g, 2) &&
      appendFloat(writer, record.accel_z_g, 2) &&
      appendFloat(writer, record.gyro_x_dps, 1) &&
      appendFloat(writer, record.gyro_y_dps, 1) &&
      appendFloat(writer, record.gyro_z_dps, 1) &&
      appendFloat(writer, record.battery_v, 3) &&
      appendFloat(writer, record.servo_power_mv, 0) &&
      appendFloat(writer, record.servo_angle_deg, 1) &&
      appendFloat(writer, record.temperature_c, 2) &&
      appendFloat(writer, record.humidity_percent, 1) &&
      appendFloat(writer, record.pressure_pa, 0) &&
      appendUnsigned(writer, record.flight_state) &&
      appendUnsigned(writer, record.error_code) &&
      appendUnsigned(writer, record.water_detected) &&
      writer.append("\n");

  if (!formatted) {
    return writer.finish(written);
  }
  return writer.finish(written);
}

}  // namespace storage
}  // namespace rocket
