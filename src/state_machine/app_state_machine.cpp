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
    __attribute__((weak)) void ui_show_screen_peer_search(void) {}
    __attribute__((weak)) void ui_show_screen_gameplay(void) {}
    __attribute__((weak)) void ui_show_screen_result(uint8_t /*local_move*/, uint8_t /*peer_move*/, uint8_t /*outcome*/) {}
    __attribute__((weak)) void ui_show_screen_error(const char* /*msg*/) {}
    __attribute__((weak)) void ui_set_status(const char* /*text*/) {}
    __attribute__((weak)) void ui_set_search_progress(uint8_t /*percent*/) {}
    __attribute__((weak)) void ui_set_search_timeout(uint8_t /*seconds_remaining*/) {}
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

    // Route any initialization failure through the explicit error event so the
    // Boot --> Error / Boot --> Halted transitions defined in state_machine.puml
    // are exercised instead of silently setting flags and continuing.
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
    // Drive BLE non-blocking state (discovery timeout only). Radio startup and
    // advertising are handled by the dedicated BLE radio task so the NimBLE
    // call tree does not share the Arduino main loop / LVGL stack.
    ble_service().update(millis());

    // Start BLE discovery on the tick *after* the Play action was dispatched,
    // so the NimBLE radio work never runs nested under lv_timer_handler().
    if (ctx_.start_discovery_pending) {
        ctx_.start_discovery_pending = false;
        // Offload the actual scan/advertise start to the dedicated task. If the
        // task is not running (e.g. Wokwi simulation) fall back to the direct
        // path, which preserves the single-player fallback behavior.
        if (ble_service().radio_task_ready()) {
            ble_service().signal_discovery_start();
        } else {
            ble_service().start_discovery();
        }
    }

    // Keep the PeerSearch timeout label in sync with the BLE discovery timer.
    update_search_timeout_display();

    // Dispatch any queued state-machine events.
    dispatch_pending();
}

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
        dispatch(ev, ctx_);
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

void AppStateMachine::update_search_timeout_display() {
    if (current() != State::PeerSearch) {
        displayed_search_timeout_ = 0xFF;
        return;
    }

    uint8_t remaining = ble_service().discovery_remaining_seconds();
    if (remaining == 0xFF) {
        // Discovery has not started yet (e.g. deferred start pending).
        return;
    }

    if (remaining != displayed_search_timeout_) {
        displayed_search_timeout_ = remaining;
        ui_set_search_timeout(remaining);
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

bool AppStateMachine::guard_game_mode_eq_MODE_MULTI_PLAYER(const Context& /*ctx*/) const {
    return ctx_.game_mode == GameMode::MULTI_PLAYER;
}

bool AppStateMachine::guard_game_mode_eq_MODE_SINGLE_PLAYER(const Context& /*ctx*/) const {
    return ctx_.game_mode == GameMode::SINGLE_PLAYER;
}

bool AppStateMachine::guard_local_move_chosen(const Context& /*ctx*/) const {
    return ctx_.local_move != Move::NONE;
}

bool AppStateMachine::guard_not_local_move_chosen(const Context& /*ctx*/) const {
    return ctx_.local_move == Move::NONE;
}

bool AppStateMachine::guard_peer_move_received(const Context& /*ctx*/) const {
    return ctx_.peer_move != Move::NONE;
}

bool AppStateMachine::guard_not_peer_move_received(const Context& /*ctx*/) const {
    return ctx_.peer_move == Move::NONE;
}

bool AppStateMachine::guard_role_host(const Context& /*ctx*/) const {
    // The role is resolved by BleService before EV_ROLE_RESOLVED is posted,
    // so the guard reads the runtime BLE role rather than the action-set
    // GameContext field. This keeps the RoleNegotiating guards independent of
    // the actions that record the role in the context.
    return ble_service().role() == Role::HOST;
}

bool AppStateMachine::guard_role_join(const Context& /*ctx*/) const {
    return ble_service().role() == Role::JOIN;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void AppStateMachine::app_init_success() {
    ui_show_screen_start();
    ui_set_status("Ready");
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
    ui_show_screen_error(ctx_.error_msg);
}

void AppStateMachine::on_play_button() {
    ctx_.reset_game();
    ui_show_screen_peer_search();
    ui_set_status("Searching for peer...");
    ui_set_search_progress(0);
    displayed_search_timeout_ = 10;
    ui_set_search_timeout(10);
    // Defer the NimBLE radio work to the next update() tick, outside the
    // LVGL timer / state-machine dispatch context.
    ctx_.start_discovery_pending = true;
}

void AppStateMachine::on_cancel_button() {
    ble_service().stop_discovery();
    ctx_.reset_game();
    ui_show_screen_start();
    ui_set_status("Cancelled");
}

void AppStateMachine::on_peer_found() {
    ui_set_status("Negotiating role...");
    // Offload role resolution to the dedicated BLE radio task so all post-
    // discovery NimBLE work runs on the radio-task stack. The task compares the
    // stored peer address with the local public address, configures the radio
    // for HOST or JOIN, and posts EV_ROLE_RESOLVED or EV_ERROR.
    ble_service().signal_become_host();
}

void AppStateMachine::start_advertising_host() {
    ctx_.role = Role::HOST;
    ui_set_status("Connecting...");
}

void AppStateMachine::start_scanning_join() {
    ctx_.role = Role::JOIN;
    ui_set_status("Connecting...");
    // Offload the connection handshake to the dedicated BLE radio task so the
    // NimBLE client/GATT call tree runs on the radio-task stack. The task posts
    // EV_CONNECTED once the link is usable, or EV_ERROR on failure.
    ble_service().signal_connect_to_host();
}

void AppStateMachine::on_peer_connected() {
    ctx_.game_mode = GameMode::MULTI_PLAYER;
    ctx_.ble_connected = true;
    ctx_.reset_round();
    ui_show_screen_gameplay();
    ui_set_status("Connected! Choose your move");
    ui_set_move_buttons_enabled(true);
}

void AppStateMachine::on_peer_disconnected() {
    ctx_.ble_connected = false;
    ctx_.role = Role::NONE;
    ble_service().disconnect();
    ui_show_screen_start();
    ui_set_status("Peer disconnected");
}

void AppStateMachine::on_discovery_timeout() {
    ctx_.game_mode = GameMode::SINGLE_PLAYER;
    ctx_.reset_round();
    ui_show_screen_gameplay();
    ui_set_status("No peer found — single player");
    ui_set_move_buttons_enabled(true);
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
        ui_set_status("Move sent — waiting for peer");
    }
}

void AppStateMachine::on_local_move_paper_pending() {
    set_local_move(Move::PAPER);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::PAPER)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
        ui_set_status("Move sent — waiting for peer");
    }
}

void AppStateMachine::on_local_move_scissors_pending() {
    set_local_move(Move::SCISSORS);
    if (ctx_.game_mode == GameMode::MULTI_PLAYER) {
        if (!ble_service().send_move(Move::SCISSORS)) {
            ::app::app_on_error("Failed to send move");
            return;
        }
        ui_set_status("Move sent — waiting for peer");
    }
}

void AppStateMachine::on_peer_move_complete() {
    // Peer move was stored by the BLE callback before posting the event.
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
    ui_set_status("Peer chose — your turn");
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
    ui_show_screen_result(
        static_cast<uint8_t>(ctx_.local_move),
        static_cast<uint8_t>(ctx_.peer_move),
        static_cast<uint8_t>(ctx_.outcome));
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
    ble_service().stop_discovery();
    ui_show_screen_start();
    ui_set_status("Ready");
}

} // namespace app
