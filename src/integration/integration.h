#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Glue layer: wire UI event callbacks to the firmware state machine.
 *
 * Must be called after ui_create() so the UI controls exist, and before the
 * first LVGL input event is processed.
 *
 * This function registers the single event-post callback used by all LVGL
 * control handlers; the callback enqueues events under the state machine's
 * FreeRTOS queue mutex.
 */
void integration_init(void);

/**
 * @brief Periodic integration update hook.
 *
 * Called from loop(); runs the state-machine update which polls BLE events and
 * dispatches pending state-machine outputs to the UI update functions.
 *
 * IMPORTANT: This MUST be called after lv_timer_handler() in loop() so that
 * deferred BLE work scheduled by a dispatched LVGL event (e.g. EV_HOST_GAME)
 * executes outside the LVGL timer context. See AppStateMachine::update().
 */
void integration_update(void);

#ifdef __cplusplus
}
#endif
