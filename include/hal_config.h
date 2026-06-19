#pragma once

// HAL configuration for CYD2USB v3 + LVGL 8.x + TFT_eSPI + XPT2046.
// This file contains only constants and macros; driver instances are created
// and initialized in the appropriate source modules.

#include "pins.h"

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
#define DISP_WIDTH  240
#define DISP_HEIGHT 320

// CYD2USB v3 native panel is 240x320 portrait (USB connector at bottom).
#define DISP_ROTATION 0

// TFT_eSPI is configured via include/User_Setup.h.
// The macros below mirror the key parameters so application code can reference
// them without depending on TFT_eSPI internals.
#define TFT_DISPLAY_WIDTH  DISP_WIDTH
#define TFT_DISPLAY_HEIGHT DISP_HEIGHT

// ---------------------------------------------------------------------------
// SPI buses
// ---------------------------------------------------------------------------
// TFT is wired to the ESP32 HSPI pins (MOSI=13, MISO=12, SCK=14).
// Touch is wired to VSPI pins (MOSI=32, MISO=39, CLK=25, CS=33).
#define TFT_SPI_BUS  HSPI
#define TOUCH_SPI_BUS VSPI

// ---------------------------------------------------------------------------
// Backlight
// ---------------------------------------------------------------------------
#define BACKLIGHT_PIN PIN_TFT_BL
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_PWM_FREQ    5000
#define BACKLIGHT_MAX_DUTY    255

// ---------------------------------------------------------------------------
// Touch defaults
// ---------------------------------------------------------------------------
// Default calibration for XPT2046 raw ADC -> screen mapping.
// These values are starting points; final calibration is performed on the
// physical CYD2USB v3 panel. Wokwi smoke tests use the ili9341 framebuffer
// and do not exercise the touch controller.
#define TOUCH_MIN_X_RAW 200
#define TOUCH_MAX_X_RAW 3700
#define TOUCH_MIN_Y_RAW 300
#define TOUCH_MAX_Y_RAW 3700

// ---------------------------------------------------------------------------
// LVGL display buffer
// ---------------------------------------------------------------------------
#define LVGL_DISP_BUF_SIZE (DISP_WIDTH * 10)

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------
// BLE is provided by the on-chip ESP32 radio; no external pins required.
#define BLE_DEVICE_NAME "CYD_RPS"
