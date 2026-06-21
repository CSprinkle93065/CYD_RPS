# Assessment: Stage 10 — Display & Input Code Critic

**Workflow:** `wvc_20260617_171607`  
**Project:** CYD_RPS v0.2.0 (minor revision)  
**Contract:** `docs/state_machine_contract.json` v1.1.0  
**Review Date:** 2026-06-21  
**Verdict:** GO

## Summary

The LVGL/touch input layer in `src/ui/` complies with the v0.2.0 state-machine contract and the UI/Display Layout section of `docs/definition.md`. All UI-bound state-machine events are emitted by the correct controls, no business logic is embedded in the UI code, there is no dynamic allocation in interrupt context or direct GPIO access, and display/touch handling contains no blocking delays.

---

## Gate Findings

### G10.1 — All contract actions are emitted by some UI control

**PASS**

Every touch/button binding in `docs/state_machine_contract.json` has a matching LVGL event handler in `src/ui/ui.cpp` that posts the expected state-machine event through the registered callback:

| Contract Control | Handler | Emitted Event | Location |
|------------------|---------|---------------|----------|
| `btnHostGame` | `ui::on_host_game_button_clicked()` | `app::Event::EV_HOST_GAME` | `src/ui/ui.cpp:77-82` |
| `btnSolo` (SCR_Start) | `ui::on_solo_button_clicked()` | `app::Event::EV_SOLO` | `src/ui/ui.cpp:84-89` |
| `btnHome` (SCR_Start / SCR_Gameplay) | `ui::on_home_button_clicked()` | `app::Event::EV_RESET` | `src/ui/ui.cpp:147-152` |
| `btnCancel` | `ui::on_cancel_host_button_clicked()` | `app::Event::EV_CANCEL_HOST` | `src/ui/ui.cpp:91-96` |
| `btnRetry` (HostTimeoutDialog) | `ui::on_host_retry_button_clicked()` | `app::Event::EV_HOST_RETRY` | `src/ui/ui.cpp:98-103` |
| `btnSolo` (HostTimeoutDialog) | `ui::on_host_solo_button_clicked()` | `app::Event::EV_SOLO` | `src/ui/ui.cpp:105-110` |
| `btnRock` | `ui::on_rock_button_clicked()` | `app::Event::EV_MOVE_ROCK` | `src/ui/ui.cpp:112-117` |
| `btnPaper` | `ui::on_paper_button_clicked()` | `app::Event::EV_MOVE_PAPER` | `src/ui/ui.cpp:119-124` |
| `btnScissors` | `ui::on_scissors_button_clicked()` | `app::Event::EV_MOVE_SCISSORS` | `src/ui/ui.cpp:126-131` |
| `btnPlayAgain` | `ui::on_play_again_button_clicked()` | `app::Event::EV_PLAY_AGAIN` | `src/ui/ui.cpp:133-138` |
| `btnRetry` (SCR_Error) | `ui::on_retry_button_clicked()` | `app::Event::EV_RETRY` | `src/ui/ui.cpp:140-145` |

All non-event adapter functions declared in the contract are also implemented and exposed in `src/ui/ui.h`:

- `ui_show_screen_start()` — `src/ui/ui.cpp:203-210`
- `ui_show_screen_host_wait()` — `src/ui/ui.cpp:212-220`
- `ui_show_host_timeout_dialog()` / `ui_hide_host_timeout_dialog()` — `src/ui/ui.cpp:222-235`
- `ui_show_screen_gameplay()` — `src/ui/ui.cpp:237-245`
- `ui_show_screen_result()` — `src/ui/ui.cpp:247-258`
- `ui_show_screen_error()` — `src/ui/ui.cpp:260-267`
- `ui_set_status()` — `src/ui/ui.cpp:269-277`
- `ui_set_device_id()` — `src/ui/ui.cpp:279-286`
- `ui_set_score()` — `src/ui/ui.cpp:294-300`
- `ui_set_host_wait_progress()` — `src/ui/ui.cpp:302-309`
- `ui_set_move_buttons_enabled()` — `src/ui/ui.cpp:311-323`

> **Note:** Serial command actions (`serial_HOST`, `serial_SOLO`, `serial_ROCK`, `serial_PAPER`, `serial_SCISSORS`, `serial_AGAIN`, `serial_RESET`, `serial_HOME`) are intentionally not part of the display/input layer. They are emitted by `src/state_machine/serial_command_handler.cpp`, which is outside the scope of this UI review.

---

### G10.2 — No business logic is embedded in UI code

**PASS**

`src/ui/` contains only presentation and input-forwarding code:

- All LVGL event handlers in `src/ui/ui.cpp` log the touch event and immediately post the corresponding state-machine event; they do not evaluate moves, compute outcomes, manage BLE state, or update the session scoreboard (`src/ui/ui.cpp:77-152`).
- `ui_move_to_string()` (`src/ui/ui.cpp:53-61`) and `ui_theme::style_outcome_label()` (`src/ui/theme.cpp:134-147`) are pure presentation helpers that map enum values to human-readable text/colors. The actual move evaluation, outcome calculation, and scoring live in `src/state_machine/game_logic.cpp` and `src/state_machine/app_state_machine.cpp`.
- `ui_show_screen_result()` (`src/ui/ui.cpp:247-258`) receives already-computed `local_move`, `peer_move`, and `outcome` values from the state machine and merely formats them on labels.
- BLE role negotiation, peer discovery, advertising control, and round evaluation are absent from `src/ui/`.

---

### G10.3 — No dynamic allocation in ISRs; GPIO access goes through the HAL/pins.h layer; watchdog-aware

**PASS**

- `src/ui/` contains no interrupt service routines.
- `touch_read_cb()` (`src/ui/touch_mapping.cpp:44-69`) is a polled LVGL input-device callback, not an ISR. It reads the pre-registered `XPT2046_Touchscreen` instance, maps coordinates, logs, and returns immediately. No `malloc`, `new`, `free`, or C++ container allocation occurs inside it.
- Display and input objects are created once at startup (`ui_create()` in `src/ui/ui.cpp:189-201` and the `create_*_screen()` functions in `src/ui/screens.cpp`). The display buffer is a static array (`static lv_color_t s_buf[DISP_WIDTH * 10];` at `src/ui/ui.cpp:31`), and the LVGL driver structs are `static` local objects.
- UI code does not access GPIO registers or raw pin numbers directly. All pin constants are defined in `include/pins.h` and referenced through `include/hal_config.h` (e.g., `TOUCH_MIN_X_RAW`, `DISP_WIDTH`). The touchscreen and display are accessed through their initialized driver classes.
- The UI layer is watchdog-aware: event handlers and the touch read callback return without blocking, and `ui_handler()` simply calls `lv_timer_handler()` so the main loop can also feed the watchdog.

---

### G10.4 — No blocking delays in display or touch input handling

**PASS**

- No `delay()`, `vTaskDelay()`, `micros()` polling loops, or busy-wait loops exist in `src/ui/`.
- LVGL event handlers return immediately after posting events (`src/ui/ui.cpp:77-152`).
- `touch_read_cb()` performs a single non-blocking `touched()` check, maps coordinates, and returns (`src/ui/touch_mapping.cpp:44-69`).
- `ui_handler()` and `ui_tick()` are thin wrappers around `lv_timer_handler()` and `lv_tick_inc()` with no added delays (`src/ui/ui.cpp:179-187`).

---

## Extra Checks

- **Layout fidelity** — Screen/widget positions, colors, fonts, and the global home button in `src/ui/screens.cpp` and `src/ui/theme.cpp` match `docs/definition.md` Section 2 (UI / Display Layout) for `SCR_Start`, `SCR_HostWait`, `SCR_Gameplay`, `SCR_Result`, `SCR_Error`, and the host-timeout dialog.
- **Touch mapping** — `touch_map_adc_to_screen()` (`src/ui/touch_mapping.cpp:26-42`) maps raw XPT2046 ADC values to screen pixels using calibration constants from `include/hal_config.h` and clamps to `[0, DISP_WIDTH-1]` / `[0, DISP_HEIGHT-1]`.
- **Event callback registration** — `ui_register_event_post_callback()` (`src/ui/ui.cpp:360-363`) provides the required integration point; the UI layer remains decoupled from the concrete state-machine queue implementation.

---

## Notes

- The contract includes serial input bindings. Those events are posted by `src/state_machine/serial_command_handler.cpp` and are therefore not expected to be emitted from `src/ui/`. This separation is consistent with the contract notes and `docs/definition.md` Section 3.
- `ui_set_peer_device_id()` is intentionally a no-op adapter; the contract marks peer-device-id display as optional, and the state machine can surface peer status through `ui_set_status()` instead.
