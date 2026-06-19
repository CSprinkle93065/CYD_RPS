/**
 * @file ui_internal.h
 * @brief Internal screen/widget handle declarations for CYD_RPS UI.
 */

#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <lvgl.h>

namespace ui {

struct StartScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblSubtitle;
    lv_obj_t* btnPlay;
    lv_obj_t* lblStatus;
};

struct PeerSearchScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblStatus;
    lv_obj_t* barSearch;
    lv_obj_t* lblTimeout;
    lv_obj_t* btnCancel;
};

struct GameplayScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblStatus;
    lv_obj_t* btnRock;
    lv_obj_t* btnPaper;
    lv_obj_t* btnScissors;
};

struct ResultScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblLocalMove;
    lv_obj_t* lblPeerMove;
    lv_obj_t* lblOutcome;
    lv_obj_t* btnPlayAgain;
};

struct ErrorScreen {
    lv_obj_t* scr;
    lv_obj_t* lblErrorTitle;
    lv_obj_t* lblErrorMsg;
    lv_obj_t* btnRetry;
};

struct ScreenManager {
    StartScreen       start;
    PeerSearchScreen  peer_search;
    GameplayScreen    gameplay;
    ResultScreen      result;
    ErrorScreen       error;
    lv_obj_t*         active_status_label;
};

extern ScreenManager g_screens;

void create_start_screen(void);
void create_peer_search_screen(void);
void create_gameplay_screen(void);
void create_result_screen(void);
void create_error_screen(void);

// LVGL event handlers defined in ui.cpp.
void on_play_button_clicked(lv_event_t* e);
void on_cancel_button_clicked(lv_event_t* e);
void on_rock_button_clicked(lv_event_t* e);
void on_paper_button_clicked(lv_event_t* e);
void on_scissors_button_clicked(lv_event_t* e);
void on_play_again_button_clicked(lv_event_t* e);
void on_retry_button_clicked(lv_event_t* e);

} // namespace ui

#endif // UI_INTERNAL_H
