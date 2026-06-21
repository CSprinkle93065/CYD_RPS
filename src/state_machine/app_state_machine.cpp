#include "app_state_machine.h"

#include <Arduino.h>

#include "ble_service.h"
#include "hal_service.h"

namespace app {

// ---------------------------------------------------------------------------
// Default weak UI adapter implementations.
// ---------------------------------------------------------------------------

extern "C" {
    __attribute__((weak)) void ui_show_screen_start(void) {}
    __attribute__((weak)) void ui_show_screen_host_wait(void) {}
    __attribute__((weak)) void ui_show_host_timeout_dialog(void) {}
    __attribute__((weak)) void ui_hide_host_timeout_dialog(void) {}
    __attribute__((weak)) void ui_show_screen_gameplay(void) {}
    __attribute__((weak)) void ui_show_screen_result(app::Move /*local_move*/, app::Move /*peer_move*/, app::Outcome /*outcome*/) {}
    __attribute__((weak)) void ui_show_screen_error(const char* /*msg*/) {}
    __attribute__((weak)) void ui_set_status(const char* /*text*/) {}
    __attribute__((weak)) void ui_set_device_id(const char* /*id*/) {}
    __attribute__((weak)) void ui_set_peer_device_id(const char* /*id*/) {}
    __attribute__((weak)) void ui_set_score(uint16_t /*wins*/, uint16_t /*losses*/, uint16_t /*draws*/) {}
    __attribute__((weak)) void ui_set_host_wait_progress(uint8_t /*percent*/) {}
    __attribute__((weak)) void ui_set_move_buttons_enabled(bool /*enabled*/) {}
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

static AppStateMachine s_sm;

AppStateMachine& sm_instance() {
    return s_sm;
}

void sm_post_event(Event ev) {
    s_sm.post_event(ev);
}

// ---------------------------------------------------------------------------
// Application helpers
// ---------------------------------------------------------------------------

void app_init() {
    // Note: actual hardware init is performed by main.cpp before app_init().
    // This function is a probe only: it records the result of that init and
    // kicks the state machine out of Boot. LL-034: no duplicate init calls.
    HalService& hal = hal_service();

    bool any_failed = hal.ble_init_failed() || hal.hw_init_failed();
    hal.mark_initialized(!any_failed);

    if (any_failed) {
        sm_post_event(Event::EV_ERROR);
    } else {
        sm_post_event(Event::EV_BOOT_DONE);
    }
}

void app_on_error(const char* msg) {
    AppStateMachine& sm = sm_instance();
    if (msg) {
        strncpy(sm.ctx().error_msg, msg, sizeof(sm.ctx().error_msg) - 1);
        sm.ctx().error_msg[sizeof(sm.ctx().error_msg) - 1] = '\0';
    }
    sm_post_event(Event::EV_ERROR);
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

AppStateMachine::AppStateMachine() : StateMachine() {
    ctx_.reset_game();
    memset(event_queue_, 0, sizeof(event_queue_));
    queue_mutex_ = xSemaphoreCreateMutex();
}

void AppStateMachine::post_event(Event ev) {
    enqueue(ev);
}

void AppStateMachine::enqueue(Event ev) {
    lock_queue();
    size_t next = (queue_tail_ + 1) % EVENT_QUEUE_SIZE;
    if (next == queue_head_) {
        overflow_ = true;
        unlock_queue();
        return;
    }
    event_queue_[queue_tail_] = ev;
    queue_tail_ = next;
    unlock_queue();
}

void AppStateMachine::update() {
    // Drive BLE non-blocking state (host timeout, etc.).
    ble_service().update(millis());

    // Keep the host-wait progress bar in sync with the BLE host timer.
    update_host_wait_progress();

    // Dispatch any queued state-machine events.
    dispatch_pending();
}

namespace {

const char* state_name(app::State state) {
    switch (state) {
        case app::State::Boot: return "Boot";
        case app::State::Start: return "Start";
        case app::State::HostWait: return "HostWait";
        case app::State::HostTimeoutDialog: return "HostTimeoutDialog";
        case app::State::Joining: return "Joining";
        case app::State::Gameplay: return "Gameplay";
        case app::State::Evaluating: return "Evaluating";
        case app::State::Result: return "Result";
        case app::State::Disconnected: return "Disconnected";
        case app::State::Error: return "Error";
        case app::State::Halted: return "Halted";
    }
    return "Unknown";
}

const char* game_mode_name(app::GameMode mode) {
    return (mode == app::GameMode::SINGLE_PLAYER) ? "SINGLE_PLAYER" : "MULTI_PLAYER";
}

} // namespace

void AppStateMachine::dispatch_pending() {
    while (true) {
        lock_queue();
        if (queue_head_ == queue_tail_) {
            unlock_queue();
            break;
        }
        Event ev = event_queue_[queue_head_];
        queue_head_ = (queue_head_ + 1) % EVENT_QUEUE_SIZE;
        unlock_queue();
        State before = current();
        dispatch(ev, ctx_);
        State after = current();
        if (after != before) {
            Serial.printf("STATE: %s\n", state_name(after));
        }
    }
}

void AppStateMachine::lock_queue() {
    if (queue_mutex_ != nullptr) {
        xSemaphoreTake(queue_mutex_, portMAX_DELAY);
    }
}

void AppStateMachine::unlock_queue() {
    if (queue_mutex_ != nullptr) {
        xSemaphoreGive(queue_mutex_);
    }
}

void AppStateMachine::update_host_wait_progress() {
    if (current() != State::HostWait) {
        displayed_host_wait_progress_ = 0xFF;
        return;
    }

    uint8_t percent = ble_service().host_wait_percent_remaining();
    if (percent != displayed_host_wait_progress_) {
        displayed_host_wait_progress_ = percent;
        ui_set_host_wait_progress(percent);
    }

    if (ble_service().host_wait_timed_out()) {
        ble_service().mark_host_wait_timeout_posted();
        post_event(Event::EV_HOST_TIMEOUT);
    }
}

// ---------------------------------------------------------------------------
// Guards
// ---------------------------------------------------------------------------

bool AppStateMachine::guard_init_ok(const Context& /*ctx*/) const {
    return hal_service().init_ok();
}

bool AppStateMachine::guard_ble_init_failed(const Context& /*ctx*/) const {
    return hal_service().ble_init_failed();
}

bool AppStateMachine::guard_hw_init_failed(const Context& /*ctx*/) const {
    return hal_service().hw_init_failed();
}

bool AppStateMachine::guard_fatal(const Context& /*ctx*/) const {
    return hal_service().fatal();
}

bool AppStateMachine::guard_host_mac_valid(const Context& /*ctx*/) const {
    return ctx_.host_mac_valid;
}

bool AppStateMachine::guard_local_mac_lower(const Context& /*ctx*/) const {
    // B01-2: deterministic symmetric role resolution. The canonical public MAC
    // is compared against the discovered Host MAC; the lower-MAC device becomes
    // Host. The peer MAC was stored in ctx_.host_mac by the discovery callback.
    if (!ctx_.host_mac_valid) {
        return false;
    }
    return memcmp(ble_service().local_public_mac(), ctx_.host_mac, 6) < 0;
}

bool AppStateMachine::guard_peer_host_seen(const Context& /*ctx*/) const {
    return ctx_.peer_host_seen;
}

bool AppStateMachine::guard_retries_exhausted(const Context& /*ctx*/) const {
    // The BLE service posts EV_CONNECT_FAILED once per failed attempt and
    // finally gives up; this guard is kept for contract compatibility.
    return true;
}

bool AppStateMachine::guard_mode_single(const Context& /*ctx*/) const {
    return ctx_.game_mode == GameMode::SINGLE_PLAYER;
}

bool AppStateMachine::guard_mode_multi_and_peer_move_received(const Context& /*ctx*/) const {
    return ctx_.game_mode == GameMode::MULTI_PLAYER && ctx_.peer_move != Move::NONE;
}

bool AppStateMachine::guard_mode_multi_and_not_peer_move_received(const Context& /*ctx*/) const {
    return ctx_.game_mode == GameMode::MULTI_PLAYER && ctx_.peer_move == Move::NONE;
}

bool AppStateMachine::guard_local_move_chosen(const Context& /*ctx*/) const {
    return ctx_.local_move != Move::NONE;
}

bool AppStateMachine::guard_not_local_move_chosen(const Context& /*ctx*/) const {
    return ctx_.local_move == Move::NONE;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void AppStateMachine::app_init_success() {
    char id[5] = {0};
    ble_service().get_device_id(id, sizeof(id));
    ui_set_device_id(id);

    ui_show_screen_start();
    ui_set_status("Ready");

    // Begin broadcasting the presence beacon and scanning for Host advertisements.
    ble_service().signal_start_presence_beacon();
}

void AppStateMachine::app_on_error() {
    show_error(ctx_.error_msg[0] ? ctx_.error_msg : "Error");
}

void AppStateMachine::app_on_error_ble() {
    show_error("BLE initialization failed");
}

void AppStateMachine::app_on_error_fatal() {
    show_error(ctx_.error_msg[0] ? ctx_.error_msg : "Fatal error");
}

void AppStateMachine::app_on_error_hw() {
    show_error("Hardware initialization failed");
}

void AppStateMachine::show_error(const char* msg) {
    if (msg) {
        strncpy(ctx_.error_msg, msg, sizeof(ctx_.error_msg) - 1);
        ctx_.error_msg[sizeof(ctx_.error_msg) - 1] = '\0';
    }
    displayed_host_wait_progress_ = 0xFF;
    ui_show_screen_error(ctx_.error_msg);
}

void AppStateMachine::on_host_game() {
    ctx_.reset_game();
    ctx_.game_mode = GameMode::MULTI_PLAYER;
    ctx_.role = Role::HOST;
    ctx_.reset_round();

    Serial.printf("MODE: %s\n", game_mode_name(ctx_.game_mode));

    ui_show_screen_host_wait();
    ui_set_status("Waiting for peer to join...");
    ui_set_host_wait_progress(100);

    // Stop the Start-screen presence beacon and become a Host advertiser.
    ble_service().become_host();
}

void AppStateMachine::on_solo() {
    ctx_.game_mode = GameMode::SINGLE_PLAYER;
    ctx_.role = Role::NONE;
    ctx_.reset_round();

    Serial.printf("MODE: %s\n", game_mode_name(ctx_.game_mode));

    ble_service().signal_stop_presence_beacon();
    ui_show_screen_gameplay();
    ui_set_status("Solo mode");
    ui_set_move_buttons_enabled(true);
}

void AppStateMachine::on_cancel_host() {
    ble_service().signal_stop_host_advertising();
    ctx_.reset_game();

    ui_show_screen_start();
    ui_set_status("Cancelled");

    // Return to presence beacon + scan.
    ble_service().signal_start_presence_beacon();
}

void AppStateMachine::on_host_timeout() {
    ui_show_host_timeout_dialog();
}

void AppStateMachine::on_host_retry() {
    ui_hide_host_timeout_dialog();
    ctx_.reset_round();
    ui_show_screen_host_wait();
    ui_set_status("Waiting for peer to join...");
    ui_set_host_wait_progress(100);
    ble_service().signal_restart_host_advertising();
}

void AppStateMachine::on_host_solo() {
    ui_hide_host_timeout_dialog();
    on_solo();
}

void AppStateMachine::on_conflict_become_join() {
    ble_service().signal_stop_presence_beacon();
    ctx_.game_mode = GameMode::MULTI_PLAYER;
    ctx_.role = Role::JOIN;
    ctx_.reset_round();

    Serial.printf("MODE: %s\n", game_mode_name(ctx_.game_mode));

    ui_show_screen_gameplay();
    ui_set_status("Joining host...");
    ui_set_move_buttons_enabled(true);

    if (ctx_.host_mac_valid) {
        ble_service().signal_connect_to_host(ctx_.host_mac);
    }
}

void AppStateMachine::on_join_failed() {
    ble_service().signal_stop_presence_beacon();
    ctx_.reset_game();
    ui_show_screen_start();
    ui_set_status("Join failed");
    ble_service().signal_start_presence_beacon();
}

void AppStateMachine::on_peer_connected() {
    ctx_.ble_connected = true;
    ctx_.reset_round();

    Serial.printf("MODE: %s\n", game_mode_name(ctx_.game_mode));

    char peer_id[5] = {0};
    ble_service().get_peer_device_id(peer_id, sizeof(peer_id));
    if (peer_id[0] != '\0') {
        ui_set_peer_device_id(peer_id);
    }

    ui_show_screen_gameplay();
    ui_set_status("Connected! Choose your move");
    ui_set_move_buttons_enabled(true);
}

void AppStateMachine::on_peer_disconnected() {
    ctx_.ble_connected = false;
    ctx_.role = Role::NONE;
    ble_service().disconnect();
    ctx_.reset_game();
    ui_show_screen_start();
    ui_set_status("Peer disconnected");
    ble_service().signal_start_presence_beacon();
}

// ---------------------------------------------------------------------------
// Move actions
// ---------------------------------------------------------------------------

void AppStateMachine::set_local_move(Move move) {
    ctx_.local_move = move;
    ui_set_move_buttons_enabled(false);
}

void AppStateMachine::evaluate_and_post() {
    ctx_.outcome = evaluate_round(ctx_.local_move, ctx_.peer_move);
    post_event(Event::EV_EVALUATE);
}

void AppStateMachine::on_local_move_rock_complete() {
    set_local_move(Move::ROCK);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::ROCK)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
    }
    evaluate_and_post();
}

void AppStateMachine::on_local_move_paper_complete() {
    set_local_move(Move::PAPER);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::PAPER)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
    }
    evaluate_and_post();
}

void AppStateMachine::on_local_move_scissors_complete() {
    set_local_move(Move::SCISSORS);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::SCISSORS)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
    }
    evaluate_and_post();
}

void AppStateMachine::on_local_move_rock_pending() {
    set_local_move(Move::ROCK);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::ROCK)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
        ui_set_status("Move sent - waiting for peer");
    }
}

void AppStateMachine::on_local_move_paper_pending() {
    set_local_move(Move::PAPER);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::PAPER)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
        ui_set_status("Move sent - waiting for peer");
    }
}

void AppStateMachine::on_local_move_scissors_pending() {
    set_local_move(Move::SCISSORS);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::SCISSORS)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
        ui_set_status("Move sent - waiting for peer");
    }
}

void AppStateMachine::on_peer_move_complete() {
    Move move = ble_service().peer_move();
    if (move != Move::NONE) {
        ctx_.peer_move = move;
    }
    evaluate_and_post();
}

void AppStateMachine::on_peer_move_pending() {
    Move move = ble_service().peer_move();
    if (move != Move::NONE) {
        ctx_.peer_move = move;
    }
    ui_set_status("Peer chose - your turn");
}

void AppStateMachine::on_singleplayer_move_rock() {
    set_local_move(Move::ROCK);
    ctx_.peer_move = random_opponent_move();
    evaluate_and_post();
}

void AppStateMachine::on_singleplayer_move_paper() {
    set_local_move(Move::PAPER);
    ctx_.peer_move = random_opponent_move();
    evaluate_and_post();
}

void AppStateMachine::on_singleplayer_move_scissors() {
    set_local_move(Move::SCISSORS);
    ctx_.peer_move = random_opponent_move();
    evaluate_and_post();
}

// ---------------------------------------------------------------------------
// Evaluation / result / new round
// ---------------------------------------------------------------------------

void AppStateMachine::evaluate_and_show_result() {
    if (ctx_.outcome == Outcome::WIN) {
        ctx_.score.wins++;
    } else if (ctx_.outcome == Outcome::LOSE) {
        ctx_.score.losses++;
    } else if (ctx_.outcome == Outcome::DRAW) {
        ctx_.score.draws++;
    }

    Serial.printf("SCORE: W%u L%u D%u\n", ctx_.score.wins, ctx_.score.losses, ctx_.score.draws);

    ui_set_score(ctx_.score.wins, ctx_.score.losses, ctx_.score.draws);
    ui_show_screen_result(ctx_.local_move, ctx_.peer_move, ctx_.outcome);
}

void AppStateMachine::start_new_round() {
    ctx_.reset_round();
    ui_show_screen_gameplay();
    ui_set_status("Choose!");
    ui_set_move_buttons_enabled(true);
}

void AppStateMachine::reset_and_return_start() {
    ctx_.reset_game();
    ble_service().disconnect();
    ble_service().signal_stop_host_advertising();
    ui_show_screen_start();
    ui_set_status("Ready");
    ble_service().signal_start_presence_beacon();
}

} // namespace app
