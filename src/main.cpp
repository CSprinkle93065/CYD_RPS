/**
 * @file main.cpp
 * @brief Application entry point for CYD_RPS.
 *
 * Owns the single initialization sequence for all hardware and subsystems
 * (serial, TFT, touch, BLE, LVGL, UI, state machine). The state machine is
 * only probed/kicked after all hardware has been initialized; it does not
 * perform duplicate initialization.
 */

#include <Arduino.h>
#include <lvgl.h>
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

// ---------------------------------------------------------------------------
// Hardware instances (owned by main.cpp; not duplicated in UI/state machine).
// ---------------------------------------------------------------------------
static TFT_eSPI tft;
static XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// ---------------------------------------------------------------------------
// LVGL display buffer
// ---------------------------------------------------------------------------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[DISP_WIDTH * 10];

static void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t*>(&color_p->full), w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

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

    // LVGL init first; UI and touch registration depend on it.
    lv_init();

    // Display init: TFT and backlight.
    tft.init();
    tft.setRotation(DISP_ROTATION);
    tft.fillScreen(TFT_BLACK);

#if defined(TFT_BL) && TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, DISP_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch init: VSPI bus, then XPT2046.
    SPI.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    ts.begin();
    ts.setRotation(0); // UI layer performs its own screen mapping.
    ui_register_touchscreen(&ts);

    // Create UI screens and wire UI events to the state machine.
    // The AppStateMachine singleton is a global static in app_state_machine.cpp,
    // so its FreeRTOS queue mutex is created before setup() and before any LVGL
    // event can be posted.
    ui_create();
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

    Serial.println("CYD_RPS setup done");
}

void loop()
{
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    // Drive LVGL first, then run the state machine/BLE update hook.
    // This ordering guarantees that EV_PLAY and other LVGL-posted events are
    // fully dispatched before any deferred NimBLE radio work runs.
    lv_timer_handler();
    integration_update();

    delay(5);
}
