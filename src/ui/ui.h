/**
 * @file ui.h
 * @brief Public UI API for CYD_RPS v0.2.0.
 *
 * The UI layer owns presentation only: screens, widgets, theme, touch input
 * mapping, and event emission. It does NOT contain business logic such as move
 * evaluation, BLE role negotiation, or scorekeeping.
 */

#ifndef UI_H
#define UI_H

#include <cstdint>
#include <lvgl.h>

#include "state_machine/state_machine_generated.h"
#include "state_machine/game_logic.h"

// Type aliases used by the state-machine contract adapter functions.
using move_t    = app::Move;
using outcome_t = app::Outcome;

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations so consumers do not need driver headers. */
class XPT2046_Touchscreen;
class TFT_eSPI;

/** Callback signature for posting state-machine events from UI controls. */
typedef void (*ui_event_post_callback_t)(app::Event event);

/**
 * @brief Initialize LVGL, register the display and touch drivers, and create
 * all screens and widgets.
 *
 * Must be called after the TFT and touch hardware have been initialized.
 */
void ui_init(TFT_eSPI* tft, XPT2046_Touchscreen* touchscreen);

/** @brief Drive LVGL's timer/tick state for the elapsed milliseconds. */
void ui_tick(uint32_t elapsed_ms);

/** @brief Run one LVGL timer handler iteration. */
void ui_handler(void);

/**
 * @brief Create all screens and widgets and bind event handlers.
 *
 * Called internally by ui_init().
 */
void ui_create(void);

/** @brief Load the start / role-select screen. */
void ui_show_screen_start(void);

/** @brief Load the hosting / waiting-for-peer screen. */
void ui_show_screen_host_wait(void);

/** @brief Load the gameplay screen and enable the move buttons. */
void ui_show_screen_gameplay(void);

/**
 * @brief Load the result screen and populate move/outcome labels.
 * @param local_move Local move value.
 * @param peer_move  Peer/opponent move value.
 * @param outcome    Round outcome value.
 */
void ui_show_screen_result(move_t local_move, move_t peer_move, outcome_t outcome);

/** @brief Load the error screen and display @p msg. */
void ui_show_screen_error(const char* msg);

/** @brief Show the host-timeout Retry/Solo dialog over SCR_HostWait. */
void ui_show_host_timeout_dialog(void);

/** @brief Hide the host-timeout Retry/Solo dialog. */
void ui_hide_host_timeout_dialog(void);

/** @brief Update the currently visible screen's primary status label. */
void ui_set_status(const char* text);

/** @brief Set the local device ID label on SCR_Start and SCR_HostWait. */
void ui_set_device_id(const char* id);

/** @brief Set the peer device ID (used in status text, optional). */
void ui_set_peer_device_id(const char* id);

/** @brief Update the session scoreboard label on gameplay/result screens. */
void ui_set_score(uint16_t wins, uint16_t losses, uint16_t draws);

/** @brief Set the host-wait timeout progress bar value (100 -> 0). */
void ui_set_host_wait_progress(uint8_t percent);

/** @brief Enable or disable the Rock/Paper/Scissors move buttons. */
void ui_set_move_buttons_enabled(bool enabled);

/** @brief Set the local-move text on the result screen (optional adapter). */
void ui_set_local_move(move_t move);

/** @brief Set the peer-move text on the result screen (optional adapter). */
void ui_set_peer_move(move_t move);

/** @brief Set the outcome text/color on the result screen (optional adapter). */
void ui_set_outcome(outcome_t outcome);

/**
 * @brief Add the global home reset button to @p screen.
 *
 * Helper used by screen creation; also exposed as a public adapter for tests
 * and custom screens.
 */
void ui_add_home_button(lv_obj_t* screen);

/**
 * @brief Register an initialized XPT2046 touchscreen as the LVGL pointer device.
 *
 * The caller retains ownership of the touchscreen instance and must initialize
 * it (including starting SPI) before calling this function.
 */
void ui_register_touchscreen(XPT2046_Touchscreen* touchscreen);

/** @brief Register the callback used to post state-machine events. */
void ui_register_event_post_callback(ui_event_post_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // UI_H
