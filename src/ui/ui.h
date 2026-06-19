/**
 * @file ui.h
 * @brief Public UI API for CYD_RPS.
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

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration so consumers do not need XPT2046_Touchscreen.h */
class XPT2046_Touchscreen;

/** Callback signature for posting state-machine events from UI controls. */
typedef void (*ui_event_post_callback_t)(app::Event event);

/**
 * @brief Create all screens and widgets and bind event handlers.
 *
 * Must be called after LVGL and the display driver have been initialized.
 */
void ui_create(void);

/** @brief Load the start / role-select screen. */
void ui_show_screen_start(void);

/** @brief Load the peer-search screen and reset its progress/timeout labels. */
void ui_show_screen_peer_search(void);

/** @brief Keep the peer-search screen visible but switch status to "Connecting...". */
void ui_show_screen_connecting(void);

/** @brief Load the gameplay screen and enable the move buttons. */
void ui_show_screen_gameplay(void);

/**
 * @brief Load the result screen and populate move/outcome labels.
 * @param local_move Local move value (MOVE_NONE/ROCK/PAPER/SCISSORS).
 * @param peer_move  Peer/opponent move value.
 * @param outcome    Outcome value (OUTCOME_NONE/WIN/LOSE/DRAW).
 */
void ui_show_screen_result(uint8_t local_move, uint8_t peer_move, uint8_t outcome);

/** @brief Load the error screen and display @p msg. */
void ui_show_screen_error(const char* msg);

/** @brief Update the currently visible screen's primary status label. */
void ui_set_status(const char* text);

/** @brief Set the peer-search progress bar value (0–100). */
void ui_set_search_progress(uint8_t percent);

/**
 * @brief Set the peer-search timeout countdown text.
 * @param seconds_remaining Seconds left before single-player fallback.
 */
void ui_set_search_timeout(uint8_t seconds_remaining);

/** @brief Enable or disable the Rock/Paper/Scissors move buttons. */
void ui_set_move_buttons_enabled(bool enabled);

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
