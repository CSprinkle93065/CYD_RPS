/**
 * @file main.cpp
 * @brief Application entry point for CYD_RPS.
 *
 * Owns the single initialization sequence for all hardware and subsystems
 * (serial, TFT, touch, BLE, UI, state machine). The state machine is only
 * probed/kicked after all hardware has been initialized; it does not perform
 * duplicate initialization.
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "pins.h"
#include "hal_config.h"
#include "ui/ui.h"
#include "state_machine/app_state_machine.h"
#include "state_machine/ble_service.h"
#include "state_machine/hal_service.h"
#include "integration/integration.h"
#include "state_machine/serial_command_handler.h"

// ---------------------------------------------------------------------------
// Hardware instances (owned by main.cpp; not duplicated in UI/state machine).
// ---------------------------------------------------------------------------
static TFT_eSPI tft;
static XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// ---------------------------------------------------------------------------
// Application entry points
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 500) {
        ;
    }
    Serial.println("CYD_RPS setup start");

    // Display init: TFT and backlight.
    tft.init();
    tft.setRotation(DISP_ROTATION);
    tft.fillScreen(TFT_BLACK);

#if defined(TFT_BL) && TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    // Touch init: VSPI bus, then XPT2046.
    SPI.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    ts.begin();
    ts.setRotation(0); // UI layer performs its own screen mapping.

    // Initialize LVGL, register display/touch drivers, and create UI screens.
    ui_init(&tft, &ts);
    integration_init();

    // BLE init: owned here exactly once; state machine probes the result.
    app::HalService& hal = app::hal_service();
#if defined(WOKWI_SIMULATION)
    // Wokwi's ESP32 emulator does not emulate NimBLE reliably: calling
    // NimBLEDevice::init() triggers a task watchdog abort on btController
    // and the device never reaches the primary screen. Skip BLE stack init
    // in simulation so boot completes and SCR_Start is rendered. Multiplayer
    // BLE is unavailable in Wokwi; it is fully enabled on physical hardware
    // (build with environment esp32-2432s028r_cyd2usb).
    Serial.println("CYD_RPS: Wokwi simulation - BLE stack skipped");
#else
    if (!app::ble_service().init()) {
        hal.mark_ble_init_failed(true);
    } else {
        app::ble_service().start_radio_task();
    }
#endif
    hal.mark_initialized(!hal.ble_init_failed() && !hal.hw_init_failed());

    // Kick the state machine out of Boot; app_init() only posts events/probes.
    app::app_init();

    // Serial command handler needs no explicit initialization; it accumulates
    // bytes and dispatches commands from loop().

    Serial.println("CYD_RPS setup done");
}

void loop()
{
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    ui_tick(now - last_tick);
    last_tick = now;

    // Drive LVGL first, then run the state-machine/BLE update hook.
    // This ordering guarantees that EV_HOST_GAME and other LVGL-posted events
    // are fully dispatched before any deferred NimBLE radio work runs.
    ui_handler();
    integration_update();

    // Accumulate and dispatch debug serial commands.
    app::serial_process_input();
    app::serial_command_dispatch();

    delay(5);
}
