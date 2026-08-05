#pragma once

#include <stddef.h>
#include <stdint.h>

#include <LoRa.h>
#include <SPI.h>

namespace rocket {
namespace drivers {

struct LoraPins {
  int sck;
  int miso;
  int mosi;
  int chip_select;
  int reset;
  int dio0;
  int dio1;
};

struct LoraPhyConfig {
  long frequency_hz;
  long signal_bandwidth_hz;
  int spreading_factor;
  int coding_rate_denominator;
  long preamble_symbols;
  uint8_t sync_word;
  int transmit_power_dbm;
  int transmit_power_output_pin;
  bool payload_crc_enabled;
  bool inverted_iq;
};

struct LoraTransmitterConfig {
  LoraPins pins;
  LoraPhyConfig phy;
};

enum class LoraPostTransmitMode : uint8_t {
  kStandby = 0,
  kReceiveContinuous = 1,
};

enum class LoraTransmitResult : uint8_t {
  kSent = 0,
  kNotReady,
  kInvalidPayload,
  kPayloadTooLarge,
  kBeginPacketFailed,
  kShortWrite,
  kEndPacketFailed,
};

// Hardware transport only. This class deliberately knows nothing about
// telemetry sync bytes, protocol versions, frame sizes or application CRC.
// Retry/backoff policy belongs to the application.
class LoraTransmitter {
 public:
  static const size_t kMaximumPayloadLength = 255;

  LoraTransmitter(SPIClass &spi,
                  LoRaClass &radio,
                  const LoraTransmitterConfig &config);

  // Initializes the dedicated SPI bus and radio, applies the configured PHY,
  // and leaves the radio in standby. Safe to call again when the application
  // decides its retry interval has elapsed.
  bool begin();

  // Stops the injected radio/SPI instance. The application decides whether
  // and when begin() should be attempted again.
  void end();

  // Synchronous transmit. SandeepMistry LoRa::endPacket(false) can block
  // indefinitely if TX_DONE never arrives; callers must account for that
  // hardware/library limitation.
  //
  // On every returning path after beginPacket(), the requested post-transmit
  // mode is restored. kStandby preserves the current flight TX-only behavior;
  // kReceiveContinuous supports a future half-duplex receive window.
  LoraTransmitResult send(
      const uint8_t *payload,
      size_t length,
      LoraPostTransmitMode post_transmit_mode =
          LoraPostTransmitMode::kStandby);

  bool enterStandby();
  bool enterReceiveContinuous();

  bool ready() const;
  const LoraTransmitterConfig &config() const;

 private:
  void applyPhyConfig();
  void restoreMode(LoraPostTransmitMode mode);

  SPIClass &spi_;
  LoRaClass &radio_;
  LoraTransmitterConfig config_;
  bool ready_;
};

}  // namespace drivers
}  // namespace rocket
