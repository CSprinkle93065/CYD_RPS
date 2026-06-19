/**
 * @file screens.cpp
 * @brief LVGL screen and widget creation for CYD_RPS.
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
                      lv_color_t bg, lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, ui_theme::text_primary(), LV_PART_MAIN);
    lv_obj_center(lbl);

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
    ui_theme::style_title_label(s.lblTitle, "CYD RPS", 24);
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 40);

    s.lblSubtitle = lv_label_create(s.scr);
    lv_label_set_text(s.lblSubtitle, "Tap Play to find a peer");
    lv_obj_set_style_text_font(s.lblSubtitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s.lblSubtitle, ui_theme::text_secondary(), LV_PART_MAIN);
    lv_obj_align(s.lblSubtitle, LV_ALIGN_TOP_MID, 0, 80);

    s.btnPlay = make_button(s.scr, "Play", 120, 60, ui_theme::accent_blue(), on_play_button_clicked);
    lv_obj_align(s.btnPlay, LV_ALIGN_TOP_MID, 0, 150);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_status_label(s.lblStatus);
    lv_obj_set_style_text_color(s.lblStatus, lv_color_hex(0xeeee00), LV_PART_MAIN); // yellow
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 250);
}

void create_peer_search_screen(void)
{
    PeerSearchScreen& s = g_screens.peer_search;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "CYD RPS", 24);
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 40);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_body_label(s.lblStatus, "Searching for peer...");
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 90);

    s.barSearch = lv_bar_create(s.scr);
    lv_obj_set_size(s.barSearch, 180, 20);
    lv_obj_align(s.barSearch, LV_ALIGN_TOP_MID, 0, 140);
    lv_bar_set_value(s.barSearch, 0, LV_ANIM_OFF);

    s.lblTimeout = lv_label_create(s.scr);
    ui_theme::style_timeout_label(s.lblTimeout);
    lv_label_set_text(s.lblTimeout, "10s remaining");
    lv_obj_align(s.lblTimeout, LV_ALIGN_TOP_MID, 0, 170);

    s.btnCancel = make_button(s.scr, "Cancel", 120, 50, ui_theme::accent_grey(), on_cancel_button_clicked);
    lv_obj_align(s.btnCancel, LV_ALIGN_TOP_MID, 0, 240);
}

void create_gameplay_screen(void)
{
    GameplayScreen& s = g_screens.gameplay;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "Choose!", 24);
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 30);

    s.lblStatus = lv_label_create(s.scr);
    ui_theme::style_status_label(s.lblStatus);
    lv_label_set_text(s.lblStatus, "Waiting for your move");
    lv_obj_align(s.lblStatus, LV_ALIGN_TOP_MID, 0, 65);

    s.btnRock     = make_button(s.scr, "\u270A Rock",     180, 50, ui_theme::accent_grey(), on_rock_button_clicked);
    s.btnPaper    = make_button(s.scr, "\u270B Paper",    180, 50, ui_theme::accent_grey(), on_paper_button_clicked);
    s.btnScissors = make_button(s.scr, "\u270C Scissors", 180, 50, ui_theme::accent_grey(), on_scissors_button_clicked);

    lv_obj_align(s.btnRock,     LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_align(s.btnPaper,    LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_align(s.btnScissors, LV_ALIGN_TOP_MID, 0, 230);
}

void create_result_screen(void)
{
    ResultScreen& s = g_screens.result;
    s.scr = lv_obj_create(NULL);
    ui_theme::apply_screen_bg(s.scr);

    s.lblTitle = lv_label_create(s.scr);
    ui_theme::style_title_label(s.lblTitle, "Result", 24);
    lv_obj_align(s.lblTitle, LV_ALIGN_TOP_MID, 0, 30);

    s.lblLocalMove = lv_label_create(s.scr);
    ui_theme::style_body_label(s.lblLocalMove, "You: -");
    lv_obj_align(s.lblLocalMove, LV_ALIGN_TOP_MID, 0, 80);

    s.lblPeerMove = lv_label_create(s.scr);
    ui_theme::style_body_label(s.lblPeerMove, "Peer: -");
    lv_obj_align(s.lblPeerMove, LV_ALIGN_TOP_MID, 0, 110);

    s.lblOutcome = lv_label_create(s.scr);
    ui_theme::style_outcome_label(s.lblOutcome, 0);
    lv_obj_align(s.lblOutcome, LV_ALIGN_TOP_MID, 0, 160);

    s.btnPlayAgain = make_button(s.scr, "Play Again", 160, 55, ui_theme::accent_blue(), on_play_again_button_clicked);
    lv_obj_align(s.btnPlayAgain, LV_ALIGN_TOP_MID, 0, 250);
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

    s.btnRetry = make_button(s.scr, "Retry", 120, 50, ui_theme::accent_grey(), on_retry_button_clicked);
    lv_obj_align(s.btnRetry, LV_ALIGN_TOP_MID, 0, 240);
}

} // namespace ui
