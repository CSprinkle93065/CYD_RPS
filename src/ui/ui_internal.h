/**
 * @file ui_internal.h
 * @brief Internal screen/widget handle declarations for CYD_RPS v0.2.0 UI.
 */

#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <lvgl.h>

namespace ui {

struct StartScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblDeviceId;
    lv_obj_t* lblStatus;
    lv_obj_t* btnHostGame;
    lv_obj_t* btnSolo;
    lv_obj_t* btnHome;
};

struct HostWaitScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblDeviceId;
    lv_obj_t* lblStatus;
    lv_obj_t* barProgress;
    lv_obj_t* btnCancel;
    lv_obj_t* btnHome;
};

struct HostTimeoutDialog {
    lv_obj_t* overlay;
    lv_obj_t* panel;
    lv_obj_t* lblTitle;
    lv_obj_t* btnRetry;
    lv_obj_t* btnSolo;
};

struct GameplayScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblScore;
    lv_obj_t* lblStatus;
    lv_obj_t* btnRock;
    lv_obj_t* btnPaper;
    lv_obj_t* btnScissors;
    lv_obj_t* btnHome;
};

struct ResultScreen {
    lv_obj_t* scr;
    lv_obj_t* lblTitle;
    lv_obj_t* lblScore;
    lv_obj_t* lblLocalMove;
    lv_obj_t* lblPeerMove;
    lv_obj_t* lblOutcome;
    lv_obj_t* btnPlayAgain;
    lv_obj_t* btnHome;
};

struct ErrorScreen {
    lv_obj_t* scr;
    lv_obj_t* lblErrorTitle;
    lv_obj_t* lblErrorMsg;
    lv_obj_t* btnRetry;
    lv_obj_t* btnHome;
};

struct ScreenManager {
    StartScreen          start;
    HostWaitScreen       host_wait;
    HostTimeoutDialog    host_timeout_dialog;
    GameplayScreen       gameplay;
    ResultScreen         result;
    ErrorScreen          error;
    lv_obj_t*            active_status_label;
    lv_obj_t*            active_score_label;
};

extern ScreenManager g_screens;

void create_start_screen(void);
void create_host_wait_screen(void);
void create_host_timeout_dialog(void);
void create_gameplay_screen(void);
void create_result_screen(void);
void create_error_screen(void);

// LVGL event handlers defined in ui.cpp.
void on_host_game_button_clicked(lv_event_t* e);
void on_solo_button_clicked(lv_event_t* e);
void on_cancel_host_button_clicked(lv_event_t* e);
void on_host_retry_button_clicked(lv_event_t* e);
void on_host_solo_button_clicked(lv_event_t* e);
void on_rock_button_clicked(lv_event_t* e);
void on_paper_button_clicked(lv_event_t* e);
void on_scissors_button_clicked(lv_event_t* e);
void on_play_again_button_clicked(lv_event_t* e);
void on_retry_button_clicked(lv_event_t* e);
void on_home_button_clicked(lv_event_t* e);

} // namespace ui

#endif // UI_INTERNAL_H
