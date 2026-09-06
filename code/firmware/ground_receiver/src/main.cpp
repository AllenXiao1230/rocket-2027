#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <LoRa.h>
#include <SPI.h>
#include <TelemetryProtocol.h>

// Production LoRa downlink receiver.
//
// The USB serial stream is a transparent binary transport for validated
// telemetry frames. Do not print diagnostics to Serial: any text would corrupt
// the ground station stream parser.

namespace {

constexpr uint32_t kSerialBaud = 115200;

constexpr int kLoRaSck = 9;
constexpr int kLoRaMiso = 10;
constexpr int kLoRaMosi = 11;
constexpr int kLoRaNss = 12;
constexpr int kLoRaDio0 = 6;
constexpr int kLoRaDio1 = 5;
constexpr int kLoRaReset = 7;

constexpr int kStatusLedPin = 48;
constexpr int kStatusLedCount = 1;

constexpr long kLoRaFrequencyHz = 433000000;
constexpr byte kLoRaSyncWord = 0x12;
constexpr long kLoRaBandwidthHz = 125E3;
constexpr int kLoRaSpreadingFactor = 9;
constexpr int kLoRaCodingRateDenominator = 6;
constexpr int kLoRaPreambleSymbols = 8;

constexpr uint32_t kRadioRetryMs = 2000;
constexpr uint32_t kLinkTimeoutMs = 3000;

SPIClass radioSpi(FSPI);
Adafruit_NeoPixel statusLed(
    kStatusLedCount, kStatusLedPin, NEO_GRB + NEO_KHZ800);

bool radioReady = false;
uint32_t lastRetryMs = 0;
uint32_t lastValidFrameMs = 0;
uint32_t validFrames = 0;
uint32_t rejectedFrames = 0;

void setLed(uint8_t red, uint8_t green, uint8_t blue) {
  statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
  statusLed.show();
}

bool initialiseRadio() {
  radioSpi.begin(kLoRaSck, kLoRaMiso, kLoRaMosi, kLoRaNss);
  LoRa.setSPI(radioSpi);
  LoRa.setPins(kLoRaNss, kLoRaReset, kLoRaDio0);

  if (!LoRa.begin(kLoRaFrequencyHz)) {
    return false;
  }

  LoRa.setSyncWord(kLoRaSyncWord);
  LoRa.setSpreadingFactor(kLoRaSpreadingFactor);
  LoRa.setSignalBandwidth(kLoRaBandwidthHz);
  LoRa.setCodingRate4(kLoRaCodingRateDenominator);
  LoRa.setPreambleLength(kLoRaPreambleSymbols);
  LoRa.enableCrc();
  LoRa.disableInvertIQ();
  LoRa.receive();
  return true;
}

void maintainRadio(uint32_t nowMs) {
  if (radioReady || nowMs - lastRetryMs < kRadioRetryMs) {
    return;
  }
  lastRetryMs = nowMs;
  radioReady = initialiseRadio();
  setLed(radioReady ? 0 : 80, radioReady ? 0 : 0, radioReady ? 24 : 0);
}

void receiveFrame(uint32_t nowMs) {
  const int packetLength = LoRa.parsePacket();
  if (packetLength <= 0) {
    return;
  }

  uint8_t frame[96];
  size_t length = 0;
  while (LoRa.available() && length < sizeof(frame)) {
    frame[length++] = static_cast<uint8_t>(LoRa.read());
  }
  while (LoRa.available()) {
    LoRa.read();
  }

  if (rocket::protocol::isValidTelemetryV2(frame, length)) {
    Serial.write(frame, length);
    validFrames++;
    lastValidFrameMs = nowMs;
    setLed(0, 48, 0);
  } else {
    rejectedFrames++;
    setLed(48, 0, 32);
  }

  LoRa.receive();
}

void updateIdleLed(uint32_t nowMs) {
  if (!radioReady) {
    setLed(80, 0, 0);
  } else if (lastValidFrameMs == 0 ||
             nowMs - lastValidFrameMs > kLinkTimeoutMs) {
    setLed(48, 24, 0);
  }
}

}  // namespace

void setup() {
  pinMode(kLoRaDio1, INPUT);
  Serial.begin(kSerialBaud);

  statusLed.begin();
  statusLed.setBrightness(32);
  setLed(48, 24, 0);

  radioReady = initialiseRadio();
  lastRetryMs = millis();
  setLed(radioReady ? 0 : 80, 0, radioReady ? 24 : 0);
}

void loop() {
  const uint32_t nowMs = millis();
  maintainRadio(nowMs);
  if (radioReady) {
    receiveFrame(nowMs);
  }

  static uint32_t lastLedUpdateMs = 0;
  if (nowMs - lastLedUpdateMs >= 250) {
    lastLedUpdateMs = nowMs;
    updateIdleLed(nowMs);
  }
  delay(1);
}
