#pragma once

// CYD_RPS v0.2.0 debug serial command handler.
//
// Reads lines from Arduino Serial and posts equivalent state-machine events.
// Commands are accepted in all builds, including Wokwi simulation, because they
// only require the standard Serial object.
//
// Supported commands (case-insensitive):
//   HOST     -> EV_HOST_GAME
//   SOLO     -> EV_SOLO
//   ROCK     -> EV_MOVE_ROCK
//   PAPER    -> EV_MOVE_PAPER
//   SCISSORS -> EV_MOVE_SCISSORS
//   AGAIN    -> EV_PLAY_AGAIN
//   RESET    -> EV_RESET
//   HOME     -> EV_RESET

#include <cstdint>
#include <cstddef>

namespace app {

// Call once per loop() to accumulate bytes from Serial.
void serial_process_input(void);

// Call once per loop() when a complete command line is available; parses and
// posts the equivalent event.
void serial_command_dispatch(void);

} // namespace app
