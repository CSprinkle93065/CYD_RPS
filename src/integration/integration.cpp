/**
 * @file integration.cpp
 * @brief Adapter/glue between the UI layer and the firmware state machine.
 *
 * This module contains no business logic. It only forwards LVGL events from
 * the UI controls to the state-machine event queue, and provides the periodic
 * hook that drives the state-machine update in the main loop.
 *
 * State-machine outputs (screen changes, status updates, move-button state)
 * are wired back to the UI through the weak UI adapter callbacks declared in
 * app_state_machine.h and implemented in the UI layer.
 *
 * Contract coverage (state_machine_contract.json):
 *   - All clickable controls (btnPlay, btnCancel, btnRock, btnPaper,
 *     btnScissors, btnPlayAgain, btnRetry) post their assigned events.
 *   - All state-driven UI adapter callbacks have strong implementations in
 *     ui.cpp, so screen/status updates are automatically wired back.
 *   - lblTimeout countdown updates are driven by the Firmware Logic agent
 *     (AppStateMachine::update_search_timeout_display() calling the weak
 *     ui_set_search_timeout() adapter), so the binding is satisfied without
 *     adding business logic to this glue layer.
 *
 * Thread safety: events are enqueued under a FreeRTOS mutex created in the
 * AppStateMachine constructor, so LVGL events and BLE callbacks can safely
 * post events while the main loop dispatches the queue.
 */

#include "integration/integration.h"

#include "ui/ui.h"
#include "state_machine/app_state_machine.h"

static void on_ui_event(app::Event event)
{
    app::sm_post_event(event);
}

void integration_init(void)
{
    ui_register_event_post_callback(on_ui_event);
}

void integration_update(void)
{
    app::sm_instance().update();
}
