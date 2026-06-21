# Assessment: Stage 3 — Definition Critic

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Revision Type:** minor_revision  
**Board Version:** CYD2USB v3 (ESP32-2432S028R)  
**Target Environment:** Physical CYD2USB v3 (primary) + Wokwi simulation (non-default)  

**Verdict:** GO

---

## Summary

`definition.md` v0.2.0 is a complete, testable, and hardware-consistent specification for the CYD_RPS minor revision. It replaces the v0.1.x automatic role-negotiation flow with an explicit Host/Join model, documents the session scoreboard, and incorporates the lessons learned from the CYD_RPS v0.1.x bug-fix runs and CYD_SmokeTest2. All seven required sections are present, every user action maps to declared API functions, signatures are deterministic, no `[Inferred]` tags remain, the hardware profile is internally consistent, and every peripheral has a declared driver/library.

---

## Gate Results

### G3.1 — All 7 required sections are present and contain sufficient detail to design a state machine and write automated tests.

**PASS**

The document contains all required sections:

1. **Application Overview** — Goals, scope, BLE topology (presence beacon, Host mode, Join mode, conflict resolution, connection timeout), and the explicit Host selection model.
2. **UI / Display Layout** — Screen properties, global home button, and detailed control tables for `SCR_Start`, `SCR_HostWait`, `SCR_Gameplay`, `SCR_Result`, and `SCR_Error`.
3. **User Actions** — 22 actions (UA-01 through UA-22) with IDs, inputs, preconditions, system responses, and API function chains.
4. **Data Model / Persistence** — Runtime data table, score update rules, BLE service/characteristic layout, protocol flow, and persistence statement.
5. **API Function List** — Application, UI, BLE, game logic, serial/debug, event handlers, and touch mapping layers with full signatures.
6. **Toolchain** — Language/framework, libraries, build flags, board parameters, simulator notes, and debug serial parser.
7. **Hardware Profile** — Board description, pin map, reserved pins, free pins, Wokwi delta, and power/connection notes.

The `game_state_t` enum (`STATE_BOOT`, `STATE_START`, `STATE_HOST_WAIT`, `STATE_GAMEPLAY`, `STATE_RESULT`, `STATE_ERROR`) provides a direct state-machine skeleton. The API list, protocol flow, and UI control tables provide enough detail to write host-simulated unit tests for game logic, serial command parsing, UI adapter contracts, and state transitions.

### G3.2 — Every User Action has at least one corresponding API function in the API Function List.

**PASS**

All 22 user actions map to one or more functions documented in Section 5:

- UA-01 → `app_init()`
- UA-02 → `on_host_game_button_clicked()`, `ble_on_host_selected()`, `ui_show_screen_host_wait()`
- UA-03 → `on_solo_button_clicked()`, `game_set_mode(...)`, `ui_show_screen_gameplay()`
- UA-04 → `ble_on_join_detected(...)`, `ble_connect_to_host(...)`, `ui_show_screen_gameplay()`
- UA-05 → `on_cancel_host_button_clicked()`, `ble_stop_host_advertising()`, `ble_start_presence_beacon()`, `ui_show_screen_start()`
- UA-06 → `ble_on_host_timeout()`, `ui_show_host_timeout_dialog()`
- UA-07 → `on_host_retry_button_clicked()`, `ui_hide_host_timeout_dialog()`, `ble_restart_host_advertising()`
- UA-08 → `on_host_solo_button_clicked()`, `ui_hide_host_timeout_dialog()`, `game_set_mode(...)`, `ui_show_screen_gameplay()`
- UA-09 → `on_rock_button_clicked()` / `on_paper_button_clicked()` / `on_scissors_button_clicked()`, `game_set_local_move(...)`, `ble_send_move(...)` / `game_set_opponent_random()`, `game_evaluate_round()`, `ui_set_move_buttons_enabled(...)`, `ui_show_screen_result(...)`
- UA-10 → `ble_on_move_received(...)`, `game_set_peer_move(...)`, `game_evaluate_round()`, `ui_show_screen_result(...)`
- UA-11 → `ble_on_move_received(...)`, `game_set_peer_move(...)`, `ui_set_status(...)`
- UA-12 → `serial_process_input()`, `ble_on_host_selected()`, `ui_show_screen_host_wait()`
- UA-13 → `serial_process_input()`, `game_set_mode(...)`, `ui_show_screen_gameplay()`
- UA-14 / UA-15 / UA-16 → `serial_process_input()`, `game_set_local_move(...)`
- UA-17 → `serial_process_input()`, `game_reset_round()`, `ui_set_move_buttons_enabled(...)`, `ui_show_screen_gameplay()`
- UA-18 → `serial_process_input()`, `on_home_button_clicked()`, `ESP.restart()`
- UA-19 → `on_play_again_button_clicked()`, `game_reset_round()`, `ui_set_move_buttons_enabled(...)`, `ui_show_screen_gameplay()`
- UA-20 → `on_home_button_clicked()`, `ESP.restart()`
- UA-21 → `ble_on_disconnected()`, `ui_show_screen_start()`
- UA-22 → `on_retry_button_clicked()`, `ble_disconnect()`, `game_reset_all()`, `ui_show_screen_start()`

No orphan actions were found.

### G3.3 — All API functions include a name and parameter signature sufficient to write a deterministic assertion.

**PASS**

Every function in Section 5 includes a C/C++ signature with typed parameters and a return type. Examples:

- `bool ble_init(void)` — assertion can check return value.
- `bool ble_connect_to_host(const uint8_t host_public_mac[6])` — assertion can verify MAC argument and return value.
- `outcome_t game_evaluate_round(void)` — assertion can check returned outcome against local/peer moves.
- `void ui_show_screen_result(move_t local, move_t peer, outcome_t outcome)` — assertion can verify the loaded screen and displayed values.
- `void touch_map_adc_to_screen(uint16_t raw_x, uint16_t raw_y, int16_t* screen_x, int16_t* screen_y)` — assertion can verify mapping from raw ADC to clamped screen pixels.

Void functions map to observable side effects (screen load, global state transition, queue post) that are testable with mocks or state inspection.

### G3.4 — No critical functional requirement remains tagged `[Inferred]`.

**PASS**

A full-text search of `definition.md` found zero `[Inferred]` tags. The Stage 2 known-issue amendment (LL-025) requiring zero inferred tags before Stage 3 is satisfied.

### G3.5 — Hardware profile is self-consistent (no pin conflicts, strapping-pin warnings documented).

**PASS**

The pin map matches the canonical CYD2USB v3 profile:

- Display (HSPI): MOSI=13, MISO=12, SCK=14, CS=15, DC=2, BL=21.
- Touch (VSPI): MOSI=32, MISO=39, CLK=25, CS=33, IRQ=36.

No pin conflicts exist: display and touch use separate SPI buses with disjoint chip-select and control pins. Input-only GPIOs 36 and 39 are used for touch IRQ and MISO (both inputs), which is valid.

Strapping-pin warnings are documented in Section 7: "Do not use GPIO 0, 2, 4, 5, 12, 15 for user peripherals without checking boot-mode impact." The onboard TFT uses GPIOs 2, 12, and 15 as part of the fixed CYD2USB v3 design; the warning correctly applies to additional user peripherals. No external user peripherals are added, so no conflict arises.

### G3.6 — Every peripheral in the Hardware Profile has a declared driver or library.

**PASS**

Section 6 (Toolchain) declares a driver/library for every peripheral:

- ST7789 display controller → `TFT_eSPI 2.5.43` (`bodmer/TFT_eSPI@^2.5.43`).
- XPT2046 resistive touchscreen → `XPT2046_Touchscreen` (`paulstoffregen/XPT2046_Touchscreen`).
- ESP32 integrated BLE radio → `NimBLE-Arduino` (`h2zero/NimBLE-Arduino@^1.4.0`).
- Backlight on GPIO 21 → controlled via Arduino-ESP32 framework GPIO/TFT_eSPI backlight support.

---

## Cross-Check Against Known Issues

The v0.2.0 definition directly addresses the recurring failure patterns recorded in `known_issues.md`:

| Known Issue | Definition Response |
|-------------|---------------------|
| `[Inferred]` tags reaching Stage 3 (LL-025) | Zero `[Inferred]` tags found. |
| Wokwi CLI does not support `wokwi-xpt2046` (LL-032) | Wokwi Delta explicitly classifies touch-driven tests as `manual`/`hardware`. |
| CYD2USB hardware initialized twice causes boot loop (LL-033) | `app_init()` is the single initialization path; no duplicate init paths are described. |
| Raw XPT2046 coordinates passed unmapped (LL-034) | `touch_read_cb` and `touch_map_adc_to_screen` are declared with clamp-to-bounds behavior. |
| Silent BLE init failure / blocking discovery (LL-035) | `ble_init()` returns `bool`; dedicated radio task owns NimBLE operations. |
| NimBLE init watchdog in Wokwi (LL-036) | `-D WOKWI_SIMULATION` flag documented; BLE init skipped in Wokwi environment. |
| Default env targets simulator and disables BLE (LL-037) | Default environment is `esp32-2432s028r_cyd2usb`; Wokwi is non-default. |
| NimBLE radio ops in LVGL/state-machine context (LL-038) | All radio work is posted to `radio_task_loop`; state-machine actions only post commands. |
| Non-thread-safe event queue (LL-039) | `event_queue_mutex` listed in runtime data. |
| UI countdown contract in integration glue (LL-040) | `ui_set_host_wait_progress(uint8_t percent)` is a dedicated UI adapter. |
| Dedicated radio task missing commands (LL-041) | `BECOME_HOST` and `CONNECT_TO_HOST` commands are explicitly listed in `radio_task_loop`. |
| GATT server started lazily during GAP procedure (LL-043) | Protocol flow step 1 requires GATT server start during `ble_init()` before any GAP procedure. |
| Peer address type discarded (LL-044) | Join mode uses `BLE_ADDR_PUBLIC` with the full public MAC from the beacon; Host uses `BLE_OWN_ADDR_PUBLIC`. |
| Asymmetric role negotiation / JOIN stops advertising (LL-045, LL-049, LL-050) | Replaced by explicit Host selection; Join remains on start screen until it detects a Host beacon, then stops presence/scan before connect. Host restarts advertising after stopping scan with a 20 ms guard delay; Join waits 50 ms after `stop_advertising()` before each connect attempt. |

---

## Minor Gaps / Observations (Non-Blocking)

1. **RNG seeding.** The definition states the single-player opponent uses a uniform RNG but does not specify the seed source (e.g., `esp_random()`, `randomSeed(analogRead(...))`). For deterministic unit tests, the seeding strategy should be documented or the RNG abstracted behind an injectable interface.

2. **Single-owner hardware initialization.** While `app_init()` is clearly the single initialization path, the definition does not explicitly state that state-machine init states must act as probes and must not re-call peripheral `init()`/`begin()` functions. Adding a one-sentence design rule would harden the specification against LL-033 recurrence.

3. **Strapping-pin note clarity.** The reserved-pins warning says "Do not use GPIO 0, 2, 4, 5, 12, 15 for user peripherals." Because GPIOs 2, 12, and 15 are used by the onboard TFT, an explicit note that "onboard peripherals use these pins as part of the fixed board design" would reduce ambiguity for future reviewers.

4. **Modal dialog controls.** The timeout dialog (UA-06/UA-07/UA-08) is described in the Application Overview and User Actions but the dialog's LVGL object names are not listed in the UI / Display Layout section. Adding control names (e.g., `dlgTimeout`, `btnRetry`, `btnSolo`) would help the Integration Agent wire the UI.

---

## Required Actions

None. The definition is sufficiently complete, testable, and hardware-consistent to proceed to Stage 4.
