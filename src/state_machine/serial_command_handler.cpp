#include "serial_command_handler.h"

#include <Arduino.h>
#include <cstring>
#include <cctype>

#include "app_state_machine.h"

namespace app {

namespace {

constexpr size_t SERIAL_BUFFER_SIZE = 32;
char s_serial_buffer[SERIAL_BUFFER_SIZE] = {0};
size_t s_serial_len = 0;
bool s_serial_ready = false;

void trim_and_uppercase(char* str) {
    if (!str) {
        return;
    }
    // Trim leading whitespace.
    char* start = str;
    while (*start && isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    // Trim trailing whitespace.
    char* end = str + strlen(str) - 1;
    while (end > start && isspace(static_cast<unsigned char>(*end))) {
        *end = '\0';
        --end;
    }
    // Move trimmed string to the front.
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    // Convert to uppercase.
    for (char* p = str; *p; ++p) {
        *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
    }
}

} // namespace

void serial_process_input(void) {
    if (s_serial_ready) {
        return;  // Wait for dispatch() to consume the current command.
    }

    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) {
            break;
        }

        if (c == '\r') {
            continue;  // Ignore CR; wait for LF.
        }
        if (c == '\n') {
            s_serial_buffer[s_serial_len] = '\0';
            s_serial_ready = true;
            s_serial_len = 0;
            return;
        }

        if (s_serial_len < SERIAL_BUFFER_SIZE - 1) {
            s_serial_buffer[s_serial_len++] = static_cast<char>(c);
        } else {
            // Overflow: drop the oldest byte to keep the line bounded.
            memmove(s_serial_buffer, s_serial_buffer + 1, SERIAL_BUFFER_SIZE - 2);
            s_serial_buffer[SERIAL_BUFFER_SIZE - 2] = static_cast<char>(c);
            s_serial_len = SERIAL_BUFFER_SIZE - 1;
        }
    }
}

void serial_command_dispatch(void) {
    if (!s_serial_ready) {
        return;
    }

    trim_and_uppercase(s_serial_buffer);

    if (s_serial_buffer[0] == '\0') {
        s_serial_ready = false;
        return;
    }

    Serial.printf("SERIAL: %s\n", s_serial_buffer);

    if (strcmp(s_serial_buffer, "HOST") == 0) {
        sm_post_event(Event::EV_HOST_GAME);
    } else if (strcmp(s_serial_buffer, "SOLO") == 0) {
        sm_post_event(Event::EV_SOLO);
    } else if (strcmp(s_serial_buffer, "ROCK") == 0) {
        sm_post_event(Event::EV_MOVE_ROCK);
    } else if (strcmp(s_serial_buffer, "PAPER") == 0) {
        sm_post_event(Event::EV_MOVE_PAPER);
    } else if (strcmp(s_serial_buffer, "SCISSORS") == 0) {
        sm_post_event(Event::EV_MOVE_SCISSORS);
    } else if (strcmp(s_serial_buffer, "AGAIN") == 0) {
        sm_post_event(Event::EV_PLAY_AGAIN);
    } else if (strcmp(s_serial_buffer, "RESET") == 0 || strcmp(s_serial_buffer, "HOME") == 0) {
        sm_post_event(Event::EV_RESET);
    } else {
        Serial.printf("SERIAL: unknown command '%s'\n", s_serial_buffer);
    }

    s_serial_ready = false;
    s_serial_buffer[0] = '\0';
}

} // namespace app
