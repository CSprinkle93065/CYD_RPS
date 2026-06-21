/**
 * @file ui.cpp
 * @brief Public UI API and LVGL event handlers for CYD_RPS v0.2.0.
 *
 * Presentation layer only. All user input is forwarded as state-machine events
 * through the registered event-post callback. No business logic is performed
 * here.
 */

#include "ui.h"
#include "ui_internal.h"
#include "theme.h"
#include "touch_mapping.h"
#include "hal_config.h"

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ---------------------------------------------------------------------------
// Event post callback registered by the integration / state-machine layer.
// ---------------------------------------------------------------------------
static ui_event_post_callback_t s_event_post_callback = nullptr;

// ---------------------------------------------------------------------------
// Display driver state (owned by the UI layer).
// ---------------------------------------------------------------------------
static TFT_eSPI* s_tft = nullptr;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_buf[DISP_WIDTH * 10];

static void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    if (s_tft == nullptr) {
        lv_disp_flush_ready(disp);
        return;
    }

    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    s_tft->startWrite();
    s_tft->setAddrWindow(area->x1, area->y1, w, h);
    s_tft->pushColors(reinterpret_cast<uint16_t*>(&color_p->full), w * h, true);
    s_tft->endWrite();

    lv_disp_flush_ready(disp);
}

namespace {

static const char* ui_move_to_string(move_t move)
{
    switch (move) {
        case move_t::ROCK:     return "Rock";
        case move_t::PAPER:    return "Paper";
        case move_t::SCISSORS: return "Scissors";
        default:               return "-";
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

void on_host_game_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Host Game button clicked");
    post_event(app::Event::EV_HOST_GAME);
}

void on_solo_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Solo button clicked");
    post_event(app::Event::EV_SOLO);
}

void on_cancel_host_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Cancel button clicked");
    post_event(app::Event::EV_CANCEL_HOST);
}

void on_host_retry_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Host Retry button clicked");
    post_event(app::Event::EV_HOST_RETRY);
}

void on_host_solo_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Host Solo button clicked");
    post_event(app::Event::EV_SOLO);
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

void on_home_button_clicked(lv_event_t* event)
{
    (void)event;
    Serial.println("UI: Home button clicked");
    post_event(app::Event::EV_RESET);
}

} // namespace ui

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ui_init(TFT_eSPI* tft, XPT2046_Touchscreen* touchscreen)
{
    s_tft = tft;

    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf, nullptr, DISP_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    touch_register_xpt2046(touchscreen);
    ui_create();
}

void ui_tick(uint32_t elapsed_ms)
{
    lv_tick_inc(elapsed_ms);
}

void ui_handler(void)
{
    lv_timer_handler();
}

void ui_create(void)
{
    ui::create_start_screen();
    ui::create_host_wait_screen();
    ui::create_host_timeout_dialog();
    ui::create_gameplay_screen();
    ui::create_result_screen();
    ui::create_error_screen();

    ui::g_screens.active_status_label = ui::g_screens.start.lblStatus;
    ui::g_screens.active_score_label  = ui::g_screens.gameplay.lblScore;
    lv_scr_load(ui::g_screens.start.scr);
}

void ui_show_screen_start(void)
{
    ui::StartScreen& s = ui::g_screens.start;
    ui::g_screens.active_status_label = s.lblStatus;
    ui::g_screens.active_score_label  = ui::g_screens.gameplay.lblScore;
    lv_label_set_text(s.lblStatus, "Ready");
    lv_scr_load(s.scr);
}

void ui_show_screen_host_wait(void)
{
    ui::HostWaitScreen& s = ui::g_screens.host_wait;
    ui::g_screens.active_status_label = s.lblStatus;
    ui::g_screens.active_score_label  = ui::g_screens.gameplay.lblScore;
    lv_label_set_text(s.lblStatus, "Waiting for peer to join...");
    lv_bar_set_value(s.barProgress, 100, LV_ANIM_OFF);
    lv_scr_load(s.scr);
}

void ui_show_host_timeout_dialog(void)
{
    ui::HostTimeoutDialog& d = ui::g_screens.host_timeout_dialog;
    lv_obj_clear_flag(d.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(d.overlay);
}

void ui_hide_host_timeout_dialog(void)
{
    ui::HostTimeoutDialog& d = ui::g_screens.host_timeout_dialog;
    lv_obj_add_flag(d.overlay, LV_OBJ_FLAG_HIDDEN);
    // Return focus to the host-wait screen.
    lv_scr_load(ui::g_screens.host_wait.scr);
}

void ui_show_screen_gameplay(void)
{
    ui::GameplayScreen& s = ui::g_screens.gameplay;
    ui::g_screens.active_status_label = s.lblStatus;
    ui::g_screens.active_score_label  = s.lblScore;
    lv_label_set_text(s.lblStatus, "Waiting for your move");
    ui_set_move_buttons_enabled(true);
    lv_scr_load(s.scr);
}

void ui_show_screen_result(move_t local_move, move_t peer_move, outcome_t outcome)
{
    ui::ResultScreen& s = ui::g_screens.result;
    ui::g_screens.active_status_label = nullptr;
    ui::g_screens.active_score_label  = s.lblScore;

    ui_set_local_move(local_move);
    ui_set_peer_move(peer_move);
    ui_set_outcome(outcome);

    lv_scr_load(s.scr);
}

void ui_show_screen_error(const char* msg)
{
    ui::ErrorScreen& s = ui::g_screens.error;
    ui::g_screens.active_status_label = nullptr;
    ui::g_screens.active_score_label  = nullptr;
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

void ui_set_device_id(const char* id)
{
    if (id == nullptr) {
        id = "----";
    }
    lv_label_set_text_fmt(ui::g_screens.start.lblDeviceId, "Device: %s", id);
    lv_label_set_text_fmt(ui::g_screens.host_wait.lblDeviceId, "Device: %s", id);
}

void ui_set_peer_device_id(const char* id)
{
    (void)id;
    // Optional: can be surfaced via ui_set_status() by the state machine.
}

void ui_set_score(uint16_t wins, uint16_t losses, uint16_t draws)
{
    if (ui::g_screens.active_score_label != nullptr) {
        lv_label_set_text_fmt(ui::g_screens.active_score_label,
                              "W:%u L:%u D:%u", wins, losses, draws);
    }
}

void ui_set_host_wait_progress(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    lv_bar_set_value(ui::g_screens.host_wait.barProgress,
                     static_cast<int32_t>(percent), LV_ANIM_OFF);
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

void ui_set_local_move(move_t move)
{
    lv_label_set_text_fmt(ui::g_screens.result.lblLocalMove,
                          "You: %s", ui_move_to_string(move));
}

void ui_set_peer_move(move_t move)
{
    lv_label_set_text_fmt(ui::g_screens.result.lblPeerMove,
                          "Peer: %s", ui_move_to_string(move));
}

void ui_set_outcome(outcome_t outcome)
{
    ui_theme::style_outcome_label(ui::g_screens.result.lblOutcome,
                                  static_cast<uint8_t>(outcome));
}

void ui_add_home_button(lv_obj_t* screen)
{
    if (screen == nullptr) {
        return;
    }
    lv_obj_t* btn = lv_btn_create(screen);
    lv_obj_t* lbl = lv_label_create(btn);
    ui_theme::style_home_button(btn, lbl);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(btn, ui::on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

void ui_register_touchscreen(XPT2046_Touchscreen* touchscreen)
{
    touch_register_xpt2046(touchscreen);
}

void ui_register_event_post_callback(ui_event_post_callback_t callback)
{
    s_event_post_callback = callback;
}
