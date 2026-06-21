#pragma once

// CYD_RPS v0.2.0 concrete state-machine engine.
//
// Derives from the generated StateMachine and implements every guard/action
// hook. The engine owns the runtime GameContext, exposes event posting
// (sm_post_event), and runs non-blocking periodic updates.

#include "state_machine_generated.h"
#include "game_logic.h"
#include "hal_service.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace app {

class AppStateMachine : public StateMachine {
public:
    AppStateMachine();

    // Event posting API matching state_machine_contract.json.
    void post_event(Event ev);

    // Non-blocking periodic update; call from loop() at ~10-100 Hz.
    void update();

    // Runtime context accessors for the integration/UI layers.
    GameContext& ctx() { return ctx_; }
    const GameContext& ctx() const { return ctx_; }

    // -----------------------------------------------------------------------
    // Generated guard hooks
    // -----------------------------------------------------------------------
    bool guard_ble_init_failed(const Context& /*ctx*/) const override;
    bool guard_fatal(const Context& /*ctx*/) const override;
    bool guard_host_mac_valid(const Context& /*ctx*/) const override;
    bool guard_hw_init_failed(const Context& /*ctx*/) const override;
    bool guard_init_ok(const Context& /*ctx*/) const override;
    bool guard_local_move_chosen(const Context& /*ctx*/) const override;
    bool guard_mode_multi_and_not_peer_move_received(const Context& /*ctx*/) const override;
    bool guard_mode_multi_and_peer_move_received(const Context& /*ctx*/) const override;
    bool guard_mode_single(const Context& /*ctx*/) const override;
    bool guard_not_local_move_chosen(const Context& /*ctx*/) const override;
    bool guard_peer_host_seen(const Context& /*ctx*/) const override;
    bool guard_retries_exhausted(const Context& /*ctx*/) const override;

    // -----------------------------------------------------------------------
    // Generated action hooks
    // -----------------------------------------------------------------------
    void app_init_success() override;
    void app_on_error() override;
    void app_on_error_ble() override;
    void app_on_error_fatal() override;
    void app_on_error_hw() override;
    void esp_restart() override;
    void evaluate_and_show_result() override;
    void on_cancel_host() override;
    void on_conflict_become_join() override;
    void on_host_game() override;
    void on_host_retry() override;
    void on_host_solo() override;
    void on_host_timeout() override;
    void on_join_failed() override;
    void on_local_move_paper_complete() override;
    void on_local_move_paper_pending() override;
    void on_local_move_rock_complete() override;
    void on_local_move_rock_pending() override;
    void on_local_move_scissors_complete() override;
    void on_local_move_scissors_pending() override;
    void on_peer_connected() override;
    void on_peer_disconnected() override;
    void on_peer_move_complete() override;
    void on_peer_move_pending() override;
    void on_singleplayer_move_paper() override;
    void on_singleplayer_move_rock() override;
    void on_singleplayer_move_scissors() override;
    void on_solo() override;
    void reset_and_return_start() override;
    void start_new_round() override;

private:
    static constexpr size_t EVENT_QUEUE_SIZE = 16;

    void dispatch_pending();
    void enqueue(Event ev);

    void set_local_move(Move move);
    void evaluate_and_post();
    void show_error(const char* msg);
    void update_host_wait_progress();

    void lock_queue();
    void unlock_queue();

    GameContext ctx_;
    Event event_queue_[EVENT_QUEUE_SIZE];
    size_t queue_head_ = 0;
    size_t queue_tail_ = 0;
    bool overflow_ = false;
    SemaphoreHandle_t queue_mutex_ = nullptr;

    uint8_t displayed_host_wait_progress_ = 0xFF;
};

// Global singleton accessors.
AppStateMachine& sm_instance();
void sm_post_event(Event ev);

// Application-level helpers (mirrors docs/definition.md).
void app_init();
void app_on_error(const char* msg);

} // namespace app

// ---------------------------------------------------------------------------
// UI adapter callbacks.
//
// The state machine calls these from its actions. Default weak no-op
// implementations are provided in app_state_machine.cpp so this stage builds
// standalone; the Display/Integration agent supplies real implementations in
// the UI layer.
// ---------------------------------------------------------------------------
extern "C" {
    __attribute__((weak)) void ui_show_screen_start(void);
    __attribute__((weak)) void ui_show_screen_host_wait(void);
    __attribute__((weak)) void ui_show_host_timeout_dialog(void);
    __attribute__((weak)) void ui_hide_host_timeout_dialog(void);
    __attribute__((weak)) void ui_show_screen_gameplay(void);
    __attribute__((weak)) void ui_show_screen_result(app::Move local_move, app::Move peer_move, app::Outcome outcome);
    __attribute__((weak)) void ui_show_screen_error(const char* msg);
    __attribute__((weak)) void ui_set_status(const char* text);
    __attribute__((weak)) void ui_set_device_id(const char* id);
    __attribute__((weak)) void ui_set_peer_device_id(const char* id);
    __attribute__((weak)) void ui_set_score(uint16_t wins, uint16_t losses, uint16_t draws);
    __attribute__((weak)) void ui_set_host_wait_progress(uint8_t percent);
    __attribute__((weak)) void ui_set_move_buttons_enabled(bool enabled);
}
