#include "drivers/lora_transmitter.h"

#include <Arduino.h>

namespace rocket {
namespace drivers {

LoraTransmitter::LoraTransmitter(
    SPIClass &spi,
    LoRaClass &radio,
    const LoraTransmitterConfig &config)
    : spi_(spi),
      radio_(radio),
      config_(config),
      ready_(false) {}

bool LoraTransmitter::begin() {
  ready_ = false;

  pinMode(config_.pins.chip_select, OUTPUT);
  digitalWrite(config_.pins.chip_select, HIGH);
  pinMode(config_.pins.reset, OUTPUT);
  digitalWrite(config_.pins.reset, HIGH);
  pinMode(config_.pins.dio0, INPUT);
  if (config_.pins.dio1 >= 0) {
    pinMode(config_.pins.dio1, INPUT);
  }

  // LoRaClass::begin() calls SPIClass::begin() without pin arguments. On the
  // ESP32 SPI implementation that call is a no-op once this custom-pin begin
  // has initialized the bus.
  spi_.begin(config_.pins.sck,
             config_.pins.miso,
             config_.pins.mosi,
             config_.pins.chip_select);
  radio_.setSPI(spi_);
  radio_.setPins(config_.pins.chip_select,
                 config_.pins.reset,
                 config_.pins.dio0);

  if (radio_.begin(config_.phy.frequency_hz) == 0) {
    return false;
  }

  applyPhyConfig();
  radio_.idle();
  ready_ = true;
  return true;
}

void LoraTransmitter::end() {
  if (ready_) {
    radio_.end();
  }
  ready_ = false;
}

LoraTransmitResult LoraTransmitter::send(
    const uint8_t *payload,
    size_t length,
    LoraPostTransmitMode post_transmit_mode) {
  if (!ready_) {
    return LoraTransmitResult::kNotReady;
  }
  if (payload == 0 || length == 0) {
    return LoraTransmitResult::kInvalidPayload;
  }
  if (length > kMaximumPayloadLength) {
    return LoraTransmitResult::kPayloadTooLarge;
  }

  // implicitHeader=false keeps the existing explicit-header PHY behavior and
  // permits transports of different application payload lengths.
  if (radio_.beginPacket(false) == 0) {
    restoreMode(post_transmit_mode);
    return LoraTransmitResult::kBeginPacketFailed;
  }

  const size_t written = radio_.write(payload, length);
  if (written != length) {
    // Do not put a partial application payload on air.
    restoreMode(post_transmit_mode);
    return LoraTransmitResult::kShortWrite;
  }

  if (radio_.endPacket(false) != 1) {
    restoreMode(post_transmit_mode);
    return LoraTransmitResult::kEndPacketFailed;
  }

  restoreMode(post_transmit_mode);
  return LoraTransmitResult::kSent;
}

bool LoraTransmitter::enterStandby() {
  if (!ready_) {
    return false;
  }
  radio_.idle();
  return true;
}

bool LoraTransmitter::enterReceiveContinuous() {
  if (!ready_) {
    return false;
  }
  radio_.receive();
  return true;
}

bool LoraTransmitter::ready() const {
  return ready_;
}

const LoraTransmitterConfig &LoraTransmitter::config() const {
  return config_;
}

void LoraTransmitter::applyPhyConfig() {
  radio_.setSyncWord(config_.phy.sync_word);
  radio_.setTxPower(config_.phy.transmit_power_dbm,
                    config_.phy.transmit_power_output_pin);
  radio_.setSpreadingFactor(config_.phy.spreading_factor);
  radio_.setSignalBandwidth(config_.phy.signal_bandwidth_hz);
  radio_.setCodingRate4(config_.phy.coding_rate_denominator);
  radio_.setPreambleLength(config_.phy.preamble_symbols);
  if (config_.phy.payload_crc_enabled) {
    radio_.enableCrc();
  } else {
    radio_.disableCrc();
  }
  if (config_.phy.inverted_iq) {
    radio_.enableInvertIQ();
  } else {
    radio_.disableInvertIQ();
  }
}

void LoraTransmitter::restoreMode(LoraPostTransmitMode mode) {
  if (mode == LoraPostTransmitMode::kReceiveContinuous) {
    radio_.receive();
  } else {
    radio_.idle();
  }
}

}  // namespace drivers
}  // namespace rocket
