/**
 * @file screens.cpp
 * @brief LVGL screen and widget creation for CYD_RPS v0.2.0.
 *
 * Screen layouts are taken from docs/definition.md Section 2 (UI / Display
 * Layout). All screens use a white-on-black theme with a dark-gray accent for
 * pressed/active states.
 */

#include "ui_internal.h"
#include "theme.h"
#include "hal_config.h"

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

ScreenManager g_screens = {};

namespace {

lv_obj_t* make_button(lv_obj_t* parent, const char* text, lv_coord_t w, lv_coord_t h,
                      lv_event_cb_t cb, bool primary)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);

    lv_obj_t* lbl = lv_label_create(btn);
    if (primary) {
        ui_theme::style_button_primary(btn, lbl, text);
    } else {
        ui_theme::style_button_secondary(btn, lbl, text);
    }

    if (cb != nullptr) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    }
    return btn;
}

} // namespace

void create_start_screen(void)
{
    StartScreen& s = g_screens.start;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "CYD RPS");
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 40);

    s.lblDeviceId = lv_label_create(s.scr);
    ui_theme::style_device_id_label(s.lblDeviceId);
    lv_label_set_text(s.lblDeviceId, "Device: ----");
    lv_obj_align(s.lblDeviceId, LV_ALIGN_TOP_MID, 0, 80);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_status_label(s.lblStatus);
    lv_label_set_text(s.lblStatus, "Ready");
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 120);

    s.btnHostGame = make_button(s.scr, "Host Game", 180, 60,
                                on_host_game_button_clicked, true);
    lv_obj_align(s.btnHostGame, LV_ALIGN_TOP_MID, 0, 170);

    s.btnSolo = make_button(s.scr, "Solo", 180, 50,
                            on_solo_button_clicked, true);
    lv_obj_align(s.btnSolo, LV_ALIGN_TOP_MID, 0, 245);

    s.btnHome = lv_btn_create(s.scr);
    ui_theme::style_home_button(s.btnHome, lv_label_create(s.btnHome));
    lv_obj_align(s.btnHome, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(s.btnHome, on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

void create_host_wait_screen(void)
{
    HostWaitScreen& s = g_screens.host_wait;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "Hosting Game");
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 40);

    s.lblDeviceId = lv_label_create(s.scr);
    ui_theme::style_device_id_label(s.lblDeviceId);
    lv_label_set_text(s.lblDeviceId, "Device: ----");
    lv_obj_align(s.lblDeviceId, LV_ALIGN_TOP_MID, 0, 80);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_status_label(s.lblStatus);
    lv_label_set_text(s.lblStatus, "Waiting for peer to join...");
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 120);

    s.barProgress = lv_bar_create(s.scr);
    lv_obj_set_size(s.barProgress, 180, 20);
    lv_obj_align(s.barProgress, LV_ALIGN_TOP_MID, 0, 160);
    lv_bar_set_value(s.barProgress, 100, LV_ANIM_OFF);
    // U04: Host wait progress bar uses blue fill on a black background.
    lv_obj_set_style_bg_color(s.barProgress, ui_theme::bg_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s.barProgress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s.barProgress, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s.barProgress, LV_OPA_COVER, LV_PART_INDICATOR);

    s.btnCancel = make_button(s.scr, "Cancel", 120, 50,
                              on_cancel_host_button_clicked, false);
    lv_obj_align(s.btnCancel, LV_ALIGN_TOP_MID, 0, 240);

    s.btnHome = lv_btn_create(s.scr);
    ui_theme::style_home_button(s.btnHome, lv_label_create(s.btnHome));
    lv_obj_align(s.btnHome, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(s.btnHome, on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

void create_host_timeout_dialog(void)
{
    HostTimeoutDialog& d = g_screens.host_timeout_dialog;

    // Full-screen overlay, initially hidden.
    d.overlay = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(d.overlay);
    lv_obj_set_size(d.overlay, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_add_flag(d.overlay, LV_OBJ_FLAG_HIDDEN);

    // Centered panel.
    d.panel = lv_obj_create(d.overlay);
    lv_obj_set_size(d.panel, 220, 160);
    lv_obj_align(d.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(d.panel, ui_theme::bg_black(), LV_PART_MAIN);
    lv_obj_set_style_border_color(d.panel, ui_theme::text_primary(), LV_PART_MAIN);
    lv_obj_set_style_border_width(d.panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(d.panel, 8, LV_PART_MAIN);

    d.lblTitle = lv_label_create(d.panel);
    ui_theme::style_body_label(d.lblTitle, "No peer joined");
    lv_obj_align(d.lblTitle, LV_ALIGN_TOP_MID, 0, 10);

    // U03: reduced button width so Retry and Solo fit side-by-side without overlap.
    d.btnRetry = make_button(d.panel, "Retry", 85, 45,
                             on_host_retry_button_clicked, true);
    lv_obj_align(d.btnRetry, LV_ALIGN_BOTTOM_LEFT, 15, -15);

    d.btnSolo = make_button(d.panel, "Solo", 85, 45,
                            on_host_solo_button_clicked, true);
    lv_obj_align(d.btnSolo, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
}

void create_gameplay_screen(void)
{
    GameplayScreen& s = g_screens.gameplay;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "Choose!");
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 30);

    s.lblScore = lv_label_create(s.scr);
    ui_theme::style_score_label(s.lblScore);
    // U01: scoreboard moved to bottom center so it no longer overlaps "Choose!".
    lv_obj_align(s.lblScore, LV_ALIGN_BOTTOM_MID, 0, -20);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_status_label(s.lblStatus);
    lv_label_set_text(s.lblStatus, "Waiting for your move");
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 70);

    s.btnRock     = make_button(s.scr, "Rock",     180, 50,
                                on_rock_button_clicked, true);
    s.btnPaper    = make_button(s.scr, "Paper",    180, 50,
                                on_paper_button_clicked, true);
    s.btnScissors = make_button(s.scr, "Scissors", 180, 50,
                                on_scissors_button_clicked, true);

    lv_obj_align(s.btnRock,     LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_align(s.btnPaper,    LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_align(s.btnScissors, LV_ALIGN_TOP_MID, 0, 230);

    s.btnHome = lv_btn_create(s.scr);
    ui_theme::style_home_button(s.btnHome, lv_label_create(s.btnHome));
    lv_obj_align(s.btnHome, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(s.btnHome, on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

void create_result_screen(void)
{
    ResultScreen& s = g_screens.result;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "Result");
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 30);

    s.lblScore = lv_label_create(s.scr);
    ui_theme::style_score_label(s.lblScore);
    // U01: scoreboard at bottom center, matching gameplay screen.
    lv_obj_align(s.lblScore, LV_ALIGN_BOTTOM_MID, 0, -20);

    s.lblLocalMove = lv_label_create(s.scr);
    ui_theme::style_body_label(s.lblLocalMove, "You: -");
    lv_obj_align(s.lblLocalMove, LV_ALIGN_TOP_MID, 0, 80);

    s.lblPeerMove = lv_label_create(s.scr);
    ui_theme::style_body_label(s.lblPeerMove, "Peer: -");
    lv_obj_align(s.lblPeerMove, LV_ALIGN_TOP_MID, 0, 110);

    s.lblOutcome = lv_label_create(s.scr);
    ui_theme::style_outcome_label(s.lblOutcome, 0);
    lv_obj_align(s.lblOutcome, LV_ALIGN_TOP_MID, 0, 160);

    s.btnPlayAgain = make_button(s.scr, "Play Again", 160, 55,
                                 on_play_again_button_clicked, true);
    // U01: Play Again moved up so it does not overlap the bottom-center scoreboard.
    lv_obj_align(s.btnPlayAgain, LV_ALIGN_TOP_MID, 0, 200);

    s.btnHome = lv_btn_create(s.scr);
    ui_theme::style_home_button(s.btnHome, lv_label_create(s.btnHome));
    lv_obj_align(s.btnHome, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(s.btnHome, on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

void create_error_screen(void)
{
    ErrorScreen& s = g_screens.error;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblErrorTitle = lv_label_create(s.scr);
    ui_theme::style_error_title(s.lblErrorTitle);
    lv_obj_align(s.lblErrorTitle, LV_ALIGN_TOP_MID, 0, 40);

    s.lblErrorMsg = lv_label_create(s.scr);
    ui_theme::style_error_msg(s.lblErrorMsg);
    lv_obj_align(s.lblErrorMsg, LV_ALIGN_CENTER, 0, 0);

    s.btnRetry = make_button(s.scr, "Retry", 120, 50,
                             on_retry_button_clicked, false);
    lv_obj_align(s.btnRetry, LV_ALIGN_TOP_MID, 0, 220);

    s.btnHome = lv_btn_create(s.scr);
    ui_theme::style_home_button(s.btnHome, lv_label_create(s.btnHome));
    lv_obj_align(s.btnHome, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(s.btnHome, on_home_button_clicked, LV_EVENT_CLICKED, nullptr);
}

} // namespace ui
