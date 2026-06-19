#pragma once

// CYD2USB v3 (ESP32-2432S028R) central pin map.
// This file contains only symbolic pin constants; no initialization code.
// Source: skills/cyd_hardware CYD2USB v3 reserved pin map.

// TFT display (HSPI)
#define PIN_TFT_MOSI  13
#define PIN_TFT_MISO  12
#define PIN_TFT_SCK   14
#define PIN_TFT_CS    15
#define PIN_TFT_DC    2
#define PIN_TFT_BL    21
#define PIN_TFT_RST   -1  // Tied to ESP32 board reset

// Touch controller (XPT2046)
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CLK  25
#define PIN_TOUCH_CS   33
#define PIN_TOUCH_IRQ  36  // Polled; set to 255 if IRQ is not used

// microSD slot (VSPI)
#define PIN_SD_MOSI 23
#define PIN_SD_MISO 19
#define PIN_SD_SCK  18
#define PIN_SD_CS   5

// RGB LED (active LOW on most boards)
#define PIN_LED_R 4
#define PIN_LED_G 16
#define PIN_LED_B 17

// Light-dependent resistor (ADC)
#define PIN_LDR 34

// Speaker / PWM
#define PIN_SPEAKER 26

// UART0 (USB serial)
#define PIN_UART_TX 1
#define PIN_UART_RX 3

// Free pins for future expansion
#define PIN_FREE_1 22
#define PIN_FREE_2 27
#define PIN_FREE_3 35
