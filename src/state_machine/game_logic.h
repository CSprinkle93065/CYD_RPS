#pragma once

// CYD_RPS v0.2.0 game logic: move / outcome / game-mode enums, evaluation rules,
// random opponent generation, and the extended runtime context used by the
// state-machine guards and actions.

#include <cstdint>
#include <cstring>

#include "state_machine_generated.h"

namespace app {

enum class Role : uint8_t {
    NONE = 0,
    HOST,   // BLE Peripheral (explicit Host selection)
    JOIN    // BLE Central (auto-join when a Host is discovered)
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

struct SessionScore {
    uint16_t wins = 0;
    uint16_t losses = 0;
    uint16_t draws = 0;
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

    // Session scoreboard.
    SessionScore score;

    // Host discovery: the public MAC of a discovered Host beacon, valid when we
    // are on SCR_Start and should auto-join.
    uint8_t host_mac[6] = {0};
    bool host_mac_valid = false;

    // Conflict-resolution flag: set when a Host sees a peer-Host advertisement
    // and should become Join.
    bool peer_host_seen = false;

    // Maximum JOIN -> HOST connection attempts (mirrors BleService).
    static constexpr int kMaxJoinRetries = 4;

    // BLE link state.
    bool ble_connected = false;

    // Error surface used by error states.
    char error_msg[64] = {0};

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
