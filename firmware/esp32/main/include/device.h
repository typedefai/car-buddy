#ifndef CAR_BUDDY_DEVICE_H_
#define CAR_BUDDY_DEVICE_H_

// ---------------------------------------------------------------------------
// Pin map for the ESP32-S3 board.
//
// Keep this file in sync with hardware/ and with AGENTS.md. All pins below are
// ESP32-S3 GPIO numbers, not module header pin numbers.
// ---------------------------------------------------------------------------

// SSD1306 OLED, 128x64, on its own I2C bus (I2C_NUM_0, 400 kHz).
// Module/board connections:
//   OLED VCC -> 3V3
//   OLED GND -> GND
//   OLED SDA -> GPIO5
//   OLED SCL -> GPIO4
#define OLED_I2C_SDA_PIN 5
#define OLED_I2C_SCL_PIN 4
#define OLED_I2C_ADDR 0x3C  // 7-bit address; some modules use 0x3D

// ---------------------------------------------------------------------------
// ES7210 + ES8311 + NS4150B codec module.
//
// !!! WIRING TRAP THAT COST A FULL DEBUG SESSION !!!
// The module silkscreen DI/DO follows the *ESP32 project's* DIN/DOUT naming,
// NOT the module's own data direction:
//   module DI = ES7210 ADC data OUTPUT  -> ESP32 I2S data INPUT  (GPIO3)
//   module DO = ES8311 DAC data INPUT   <- ESP32 I2S data OUTPUT (GPIO18)
// Consequently there is NO crossover when matching the vendor reference wiring:
//   ESP32 DIN  <-> module DI
//   ESP32 DOUT <-> module DO
// If these are reversed, I2C detection and codec init still succeed and all
// clocks look normal, but the speaker is completely silent and the microphone
// reads ~0. Measured symptom on a crossed board:
//   DI ~0.9 V  (two outputs fighting; ESP32 was driving the ADC output)
//   DO ~1.2 V  (floating ESP32 input; ES8311 DAC input got no data)
// Do not assume "DI = data into the module". Check the vendor reference table.
//
// 10-pin module header (as labeled on the module):
//   pin  1  GND  -> ESP32 GND (common ground is required)
//   pin  2  VCC  -> 3V3 or 5V. 5V is recommended for full speaker volume.
//                   If the board's 5V pin is a phantom voltage (some ESP32-S3
//                   boards need the IN-OUT solder bridge closed), use 3V3.
//   pin  3  EN   -> GPIO13, NS4150B enable. HIGH = amplifier on, LOW = off.
//   pin  4  SDA  -> GPIO12, codec I2C data (I2C_NUM_1, 100 kHz)
//   pin  5  SCL  -> GPIO11, codec I2C clock
//   pin  6  MCK  -> GPIO10, I2S master clock (256 * fs = 4.096 MHz at 16 kHz)
//   pin  7  BCK  -> GPIO9,  I2S bit clock
//   pin  8  DO   -> GPIO18, ESP32 output -> module (I2S playback data)
//   pin  9  WS   -> GPIO17, I2S word select / LRCK
//   pin 10  DI   -> GPIO3,  module -> ESP32 input  (I2S record data)
//
// The ES7210 sends all four mic channels as TDM on DI; the ES8311 receives
// stereo playback on DO. The ES7210 is at 7-bit I2C 0x41 (8-bit 0x82) on this
// module; the ES8311 is at 7-bit I2C 0x18 (8-bit 0x30).
// ---------------------------------------------------------------------------
#define CODEC_I2C_SDA 12   // module pin 4  SDA
#define CODEC_I2C_SCL 11   // module pin 5  SCL
#define CODEC_I2S_MCLK 10  // module pin 6  MCK
#define CODEC_I2S_BCLK 9   // module pin 7  BCK
#define CODEC_I2S_WS 17    // module pin 9  WS
#define CODEC_I2S_DOUT 18  // module pin 8  DO  (ESP32 output -> module)
#define CODEC_I2S_DIN 3    // module pin 10 DI  (module -> ESP32 input)
#define CODEC_PA_EN 13     // module pin 3  EN  (high = NS4150B enabled)

// WS2812 RGB LED data input (single-wire, driven over RMT).
// LED VCC -> 3V3 or 5V, LED GND -> GND.
#define RGB_LED_GPIO 48

// UART link to the STM32 (STM32 uses USART2: PA2=TX, PA3=RX, 115200 8N1).
// ESP32 GPIO14 (TX) -> STM32 PA3 (USART2_RX)
// ESP32 GPIO21 (RX) <- STM32 PA2 (USART2_TX)
// Connect GND on both boards.
#define UART_STM32_TX 14
#define UART_STM32_RX 21

#endif  // CAR_BUDDY_DEVICE_H_
