/**
 * @file ui.cpp
 * @brief Public UI API and LVGL event handlers for CYD_RPS.
 *
 * Presentation layer only. All user input is forwarded as state-machine events
 * through the registered event-post callback.
 */

#include "ui.h"
#include "ui_internal.h"
#include "theme.h"
#include "touch_mapping.h"

#include <Arduino.h>
#include <lvgl.h>

// ---------------------------------------------------------------------------
// Event post callback registered by the integration / state-machine layer.
// ---------------------------------------------------------------------------
static ui_event_post_callback_t s_event_post_callback = nullptr;

namespace {

static const char* move_to_string(uint8_t move)
{
    switch (move) {
        case 1: return "Rock";
        case 2: return "Paper";
        case 3: return "Scissors";
        default: return "-";
    }
}

static void post_event(app::Event event)
{
    if (s_event_post_callback != nullptr) {
        s_event_post_callback(event);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// LVGL event handlers: log and emit contract actions. No business logic.
// ---------------------------------------------------------------------------
namespace ui {

void on_play_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Play button clicked");
    post_event(app::Event::EV_PLAY);
}

void on_cancel_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Cancel button clicked");
    post_event(app::Event::EV_CANCEL);
}

void on_rock_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Rock button clicked");
    post_event(app::Event::EV_MOVE_ROCK);
}

void on_paper_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Paper button clicked");
    post_event(app::Event::EV_MOVE_PAPER);
}

void on_scissors_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Scissors button clicked");
    post_event(app::Event::EV_MOVE_SCISSORS);
}

void on_play_again_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Play Again button clicked");
    post_event(app::Event::EV_PLAY_AGAIN);
}

void on_retry_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Retry button clicked");
    post_event(app::Event::EV_RETRY);
}

} // namespace ui

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ui_create(void)
{
    ui::create_start_screen();
    ui::create_peer_search_screen();
    ui::create_gameplay_screen();
    ui::create_result_screen();
    ui::create_error_screen();

    ui::g_screens.active_status_label = ui::g_screens.start.lblStatus;
    lv_scr_load(ui::g_screens.start.scr);
}

void ui_show_screen_start(void)
{
    ui::g_screens.active_status_label = ui::g_screens.start.lblStatus;
    lv_scr_load(ui::g_screens.start.scr);
    lv_label_set_text(ui::g_screens.start.lblStatus, "Ready");
}

void ui_show_screen_peer_search(void)
{
    ui::PeerSearchScreen& s = ui::g_screens.peer_search;
    ui::g_screens.active_status_label = s.lblStatus;
    lv_bar_set_value(s.barSearch, 0, LV_ANIM_OFF);
    lv_label_set_text(s.lblStatus, "Searching for peer...");
    lv_label_set_text(s.lblTimeout, "10s remaining");
    lv_scr_load(s.scr);
}

void ui_show_screen_connecting(void)
{
    ui::PeerSearchScreen& s = ui::g_screens.peer_search;
    ui::g_screens.active_status_label = s.lblStatus;
    lv_label_set_text(s.lblStatus, "Connecting...");
    lv_bar_set_value(s.barSearch, 50, LV_ANIM_OFF);
    // Keep peer-search screen visible; only the status changes.
    lv_scr_load(s.scr);
}

void ui_show_screen_gameplay(void)
{
    ui::GameplayScreen& s = ui::g_screens.gameplay;
    ui::g_screens.active_status_label = s.lblStatus;
    lv_label_set_text(s.lblStatus, "Waiting for your move");
    ui_set_move_buttons_enabled(true);
    lv_scr_load(s.scr);
}

void ui_show_screen_result(uint8_t local_move, uint8_t peer_move, uint8_t outcome)
{
    ui::ResultScreen& s = ui::g_screens.result;
    ui::g_screens.active_status_label = nullptr;

    lv_label_set_text_fmt(s.lblLocalMove, "You: %s", move_to_string(local_move));
    lv_label_set_text_fmt(s.lblPeerMove, "Peer: %s", move_to_string(peer_move));
    ui_theme::style_outcome_label(s.lblOutcome, outcome);

    lv_scr_load(s.scr);
}

void ui_show_screen_error(const char* msg)
{
    ui::ErrorScreen& s = ui::g_screens.error;
    ui::g_screens.active_status_label = nullptr;
    lv_label_set_text(s.lblErrorMsg, msg != nullptr ? msg : "Unknown error");
    lv_scr_load(s.scr);
}

void ui_set_status(const char* text)
{
    if (text == nullptr) {
        return;
    }
    if (ui::g_screens.active_status_label != nullptr) {
        lv_label_set_text(ui::g_screens.active_status_label, text);
    }
}

void ui_set_search_progress(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    lv_bar_set_value(ui::g_screens.peer_search.barSearch, static_cast<int32_t>(percent), LV_ANIM_OFF);
}

void ui_set_search_timeout(uint8_t seconds_remaining)
{
    lv_label_set_text_fmt(ui::g_screens.peer_search.lblTimeout, "%us remaining", seconds_remaining);
}

void ui_set_move_buttons_enabled(bool enabled)
{
    ui::GameplayScreen& s = ui::g_screens.gameplay;
    if (enabled) {
        lv_obj_clear_state(s.btnRock,     LV_STATE_DISABLED);
        lv_obj_clear_state(s.btnPaper,    LV_STATE_DISABLED);
        lv_obj_clear_state(s.btnScissors, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s.btnRock,     LV_STATE_DISABLED);
        lv_obj_add_state(s.btnPaper,    LV_STATE_DISABLED);
        lv_obj_add_state(s.btnScissors, LV_STATE_DISABLED);
    }
}

void ui_register_touchscreen(XPT2046_Touchscreen* touchscreen)
{
    touch_register_xpt2046(touchscreen);
}

void ui_register_event_post_callback(ui_event_post_callback_t callback)
{
    s_event_post_callback = callback;
}
