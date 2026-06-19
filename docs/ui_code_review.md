# Assessment: Stage 10 — Display & Input Code Critic

**Verdict:** GO

## Findings

- [PASS] G10.1 — All contract actions are emitted by some UI control.
  - `btnPlay` → `on_play_button_clicked()` → `post_event(app::Event::EV_PLAY)` (`src/ui/ui.cpp:52`).
  - `btnCancel` → `on_cancel_button_clicked()` → `post_event(app::Event::EV_CANCEL)` (`src/ui/ui.cpp:59`).
  - `btnRock` → `on_rock_button_clicked()` → `post_event(app::Event::EV_MOVE_ROCK)` (`src/ui/ui.cpp:66`).
  - `btnPaper` → `on_paper_button_clicked()` → `post_event(app::Event::EV_MOVE_PAPER)` (`src/ui/ui.cpp:73`).
  - `btnScissors` → `on_scissors_button_clicked()` → `post_event(app::Event::EV_MOVE_SCISSORS)` (`src/ui/ui.cpp:80`).
  - `btnPlayAgain` → `on_play_again_button_clicked()` → `post_event(app::Event::EV_PLAY_AGAIN)` (`src/ui/ui.cpp:87`).
  - `btnRetry` → `on_retry_button_clicked()` → `post_event(app::Event::EV_RETRY)` (`src/ui/ui.cpp:94`).
  - These match the `bound_state_machine_action` entries in `docs/state_machine_contract.json` for controls `btnPlay`, `btnCancel`, `btnRock`, `btnPaper`, `btnScissors`, `btnPlayAgain`, and `btnRetry`.

- [PASS] G10.2 — No business logic is embedded in UI code.
  - `src/ui/ui.cpp` event handlers only log and emit state-machine events.
  - `move_to_string()` (`src/ui/ui.cpp:24`) is a pure display-string conversion helper, not game logic.
  - `style_outcome_label()` (`src/ui/theme.cpp:99`) maps an outcome enum to text/color for presentation; move evaluation and outcome calculation live in `src/state_machine/game_logic.cpp` and `src/state_machine/app_state_machine.cpp`.
  - BLE role negotiation, peer discovery, scoring, and round evaluation are absent from `src/ui/`.

- [PASS] G10.3 — No dynamic allocation in ISRs; GPIO access goes through the HAL/pins.h layer; watchdog-aware.
  - `src/ui/` contains no ISRs.
  - `touch_read_cb()` (`src/ui/touch_mapping.cpp:44`) is a polled LVGL input-device callback, not an ISR; it reads the registered `XPT2046_Touchscreen` instance and returns immediately.
  - UI code does not access GPIO registers or raw pin numbers directly; it depends on LVGL and the pre-initialized touchscreen driver.
  - `include/pins.h` provides the CYD2USB v3 symbolic pin map and is included via `include/hal_config.h` where needed.

- [PASS] G10.4 — No blocking delays in display or touch input handling.
  - No `delay()` calls exist in `src/ui/`.
  - LVGL event handlers in `src/ui/ui.cpp` return immediately after posting events.
  - `touch_read_cb()` performs a single non-blocking touch check, maps coordinates, and returns.

## Extra Checks

- [PASS] LL-034 — No duplicate TFT/touch initialization in UI code.
  - `src/ui/` never calls `tft.init()`, `ts.begin()`, `SPI.begin()`, or `lv_init()`.
  - The single hardware initialization path is in `src/main.cpp` (`tft.init()`, `SPI.begin()`, `ts.begin()`), and `src/state_machine/hal_service.h` explicitly states it does not initialize display/touch/BLE hardware.

- [PASS] LL-035 — Touch mapping layer maps raw ADC to screen pixels and clamps.
  - `touch_map_adc_to_screen()` (`src/ui/touch_mapping.cpp:26`) uses `map()` with calibration constants `TOUCH_MIN_X_RAW`/`TOUCH_MAX_X_RAW`/`TOUCH_MIN_Y_RAW`/`TOUCH_MAX_Y_RAW` and clamps the result to `[0, DISP_WIDTH-1]` and `[0, DISP_HEIGHT-1]`.
  - `touch_read_cb()` invokes the mapping before reporting coordinates to LVGL.

## Notes

- `ui_register_event_post_callback()` is declared and implemented in `src/ui/ui.h`/`src/ui/ui.cpp`, but the temporary `src/main.cpp` stub does not register a callback. This is an integration wiring gap for Stage 13; the UI layer itself is contract-compliant and ready to be wired to `app::sm_post_event` or `AppStateMachine::post_event`.
- `ui_show_screen_connecting()` and `ui_set_search_timeout()` are present in the UI API but are not referenced by the generated state machine or the contract bindings. They are harmless additions and do not violate the contract.
