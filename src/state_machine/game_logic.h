#pragma once

// CYD_RPS game logic: move / outcome / game-mode enums, evaluation rules,
// random opponent generation, and the extended runtime context used by the
// state-machine guards and actions.

#include <cstdint>
#include <cstring>

#include "state_machine_generated.h"

namespace app {

enum class Role : uint8_t {
    NONE = 0,
    NEGOTIATING,
    HOST,   // BLE Peripheral (lower MAC address)
    JOIN    // BLE Central (higher MAC address)
};

enum class GameMode : uint8_t {
    SINGLE_PLAYER = 0,
    MULTI_PLAYER
};

enum class Move : uint8_t {
    NONE = 0,
    ROCK = 1,
    PAPER = 2,
    SCISSORS = 3
};

enum class Outcome : uint8_t {
    NONE = 0,
    WIN,
    LOSE,
    DRAW
};

// Extended context used by the generated StateMachine guards and actions.
// Derives from the generated empty Context so it can be passed to dispatch().
struct GameContext : public Context {
    // Hardware / init probes (recorded by hal_service, read by guards).
    bool init_ok = false;
    bool ble_init_failed = false;
    bool hw_init_failed = false;
    bool fatal = false;

    // Opponent source and BLE role.
    GameMode game_mode = GameMode::MULTI_PLAYER;
    Role role = Role::NONE;

    // Round state.
    Move local_move = Move::NONE;
    Move peer_move = Move::NONE;
    Outcome outcome = Outcome::NONE;

    // BLE link state.
    bool ble_connected = false;

    // Error surface used by error states.
    char error_msg[64] = {0};

    // Deferred work scheduled by an LVGL-driven action and executed on the
    // next state-machine update tick (after lv_timer_handler() has returned).
    bool start_discovery_pending = false;

    void reset_round() {
        local_move = Move::NONE;
        peer_move = Move::NONE;
        outcome = Outcome::NONE;
    }

    void reset_game() {
        reset_round();
        game_mode = GameMode::MULTI_PLAYER;
        role = Role::NONE;
        ble_connected = false;
        fatal = false;
        error_msg[0] = '\0';
        start_discovery_pending = false;
    }
};

// String representations for UI and logging.
const char* move_to_string(Move move);
const char* outcome_to_string(Outcome outcome);

// Evaluate one round of rock-paper-scissors.
Outcome evaluate_round(Move local, Move peer);

// Generate a uniform random opponent move (ROCK/PAPER/SCISSORS).
Move random_opponent_move();

} // namespace app
