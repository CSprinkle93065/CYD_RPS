#include "game_logic.h"

#include <Arduino.h>

namespace app {

const char* move_to_string(Move move) {
    switch (move) {
        case Move::ROCK:     return "Rock";
        case Move::PAPER:    return "Paper";
        case Move::SCISSORS: return "Scissors";
        default:             return "-";
    }
}

const char* outcome_to_string(Outcome outcome) {
    switch (outcome) {
        case Outcome::WIN:  return "WIN";
        case Outcome::LOSE: return "LOSE";
        case Outcome::DRAW: return "DRAW";
        default:            return "-";
    }
}

Outcome evaluate_round(Move local, Move peer) {
    if (local == Move::NONE || peer == Move::NONE) {
        return Outcome::NONE;
    }
    if (local == peer) {
        return Outcome::DRAW;
    }
    if ((local == Move::ROCK     && peer == Move::SCISSORS) ||
        (local == Move::PAPER    && peer == Move::ROCK) ||
        (local == Move::SCISSORS && peer == Move::PAPER)) {
        return Outcome::WIN;
    }
    return Outcome::LOSE;
}

Move random_opponent_move() {
    // esp_random() returns a hardware-backed 32-bit value.
    uint32_t r = esp_random();
    uint32_t v = (r % 3) + 1;  // map to 1..3
    return static_cast<Move>(v);
}

} // namespace app
