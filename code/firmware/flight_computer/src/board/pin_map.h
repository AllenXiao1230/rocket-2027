#pragma once

// Rocket 2027 flight-computer PCB pin map.
//
// Keep all physical pin ownership here. Logic modules must receive values
// through snapshots/interfaces rather than including this board header.

#define I2C_A_SDA 20
#define I2C_A_SCL 19
#define I2C_B_SDA 9
#define I2C_B_SCL 10

#define UART_GPS_TX 35
#define UART_GPS_RX 45
#define UART_GPS_PPS 47
#define UART_GPS_BAUD 230400

#define UART_SUB_TX 17
#define UART_SUB_RX 18
#define UART_SUB_BAUD 115200

#define LORA_SCK 42
#define LORA_MISO 41
#define LORA_MOSI 40
#define LORA_NSS 39
#define LORA_RST 38
#define LORA_DIO0 37
#define LORA_DIO1 36
#define LORA_FREQ 433000000
#define LORA_SYNC_WORD 0x12
#define LORA_TX_POWER 22
#define LORA_BW 125000L
#define LORA_SF 9
#define LORA_CR 6
#define LORA_PREAMBLE_LEN 8

#define SD_CS 11
#define SD_MOSI 12
#define SD_SCK 13
#define SD_MISO 14

#define BUZZER_PIN 8
#define BUZZER_CH 0
#define BUZZER_RES 10

#define WS2812_PIN 48
#define WS2812_COUNT 1

#define BATTERY_ADC_PIN 4
#define DS3231_SQW_PIN 3
#define WATER_DETECT_PIN 15
#define SAFETY_SWITCH_PIN 21

// ObserveOnly outputs: these remain inactive in Rocket 2027.
#define SEPARATION_SERVO_PIN 16
#define CAMERA_A_TRIGGER_PIN 5
#define CAMERA_B_TRIGGER_PIN 6

