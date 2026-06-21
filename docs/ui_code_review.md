# Assessment: Stage 10 — Display & Input Code Critic

**Workflow:** `wvc_20260618_011606`  
**Project:** CYD_RPS v0.2.1  
**Contract:** `docs/state_machine_contract.json` v1.1.0  
**Bug Report:** `docs/BugReport_CYD_RPS_v0.2.0.md`  
**Review Date:** 2026-06-21  
**Verdict:** GO

## Summary

The Stage 9 UI changes in `src/ui/` correctly address UI issues U01–U05 from the v0.2.0 bug report and remain compliant with the state-machine contract. All UI-bound state-machine events are emitted by the correct controls, no business logic is embedded in the UI code, there is no dynamic allocation in interrupt context or direct GPIO access, and display/touch handling contains no blocking delays.

## Gate Findings

### G10.1 — All contract actions are emitted by some UI control

**[PASS]** Every LVGL `LV_EVENT_CLICKED` binding in `docs/state_machine_contract.json` has a matching event handler in `src/ui/ui.cpp` that posts the expected state-machine event through the registered callback:

| Contract Control | Handler | Emitted Event | Location |
|------------------|---------|---------------|----------|
| `btnHostGame` | `ui::on_host_game_button_clicked()` | `app::Event::EV_HOST_GAME` | `src/ui/ui.cpp:85-90` |
| `btnSolo` (SCR_Start) | `ui::on_solo_button_clicked()` | `app::Event::EV_SOLO` | `src/ui/ui.cpp:92-97` |
| `btnHome` (SCR_Start / SCR_Gameplay) | `ui::on_home_button_clicked()` | `app::Event::EV_RESET` | `src/ui/ui.cpp:155-160` |
| `btnCancel` | `ui::on_cancel_host_button_clicked()` | `app::Event::EV_CANCEL_HOST` | `src/ui/ui.cpp:99-104` |
| `btnRetry` (HostTimeoutDialog) | `ui::on_host_retry_button_clicked()` | `app::Event::EV_HOST_RETRY` | `src/ui/ui.cpp:106-111` |
| `btnSolo` (HostTimeoutDialog) | `ui::on_host_solo_button_clicked()` | `app::Event::EV_SOLO` | `src/ui/ui.cpp:113-118` |
| `btnRock` | `ui::on_rock_button_clicked()` | `app::Event::EV_MOVE_ROCK` | `src/ui/ui.cpp:120-125` |
| `btnPaper` | `ui::on_paper_button_clicked()` | `app::Event::EV_MOVE_PAPER` | `src/ui/ui.cpp:127-132` |
| `btnScissors` | `ui::on_scissors_button_clicked()` | `app::Event::EV_MOVE_SCISSORS` | `src/ui/ui.cpp:134-139` |
| `btnPlayAgain` | `ui::on_play_again_button_clicked()` | `app::Event::EV_PLAY_AGAIN` | `src/ui/ui.cpp:141-146` |
| `btnRetry` (SCR_Error) | `ui::on_retry_button_clicked()` | `app::Event::EV_RETRY` | `src/ui/ui.cpp:148-153` |

All non-event adapter functions declared in the contract are also implemented in `src/ui/ui.h` / `src/ui/ui.cpp`.

> **Note:** Serial command actions (`serial_HOST`, `serial_SOLO`, `serial_ROCK`, `serial_PAPER`, `serial_SCISSORS`, `serial_AGAIN`, `serial_RESET`, `serial_HOME`) are intentionally not part of the display/input layer. They are emitted by the serial command handler, which is outside the scope of this UI review.

### G10.2 — No business logic is embedded in UI code

**[PASS]** `src/ui/` contains only presentation and input-forwarding code:

- All LVGL event handlers in `src/ui/ui.cpp` log the touch event and immediately post the corresponding state-machine event; they do not evaluate moves, compute outcomes, manage BLE state, or update the session scoreboard (`src/ui/ui.cpp:85-160`).
- `ui_move_to_string()` (`src/ui/ui.cpp:61-69`) and `ui_theme::style_outcome_label()` (`src/ui/theme.cpp:134-147`) are pure presentation helpers that map enum values to human-readable text. The actual move evaluation, outcome calculation, and scoring live in `src/state_machine/game_logic.cpp` and `src/state_machine/app_state_machine.cpp`.
- `ui_show_screen_result()` (`src/ui/ui.cpp:255-269`) receives already-computed `local_move`, `peer_move`, and `outcome` values from the state machine and merely formats them on labels.
- The score cache added for U05 (`src/ui/ui.cpp:30-32` and `src/ui/ui.cpp:305-316`) does not compute scores; it only stores the last values supplied by `ui_set_score()` so the result screen can display the same live score as the gameplay screen.
- BLE role negotiation, peer discovery, advertising control, and round evaluation are absent from `src/ui/`.

### G10.3 — No dynamic allocation in ISRs; GPIO access goes through the HAL/pins.h layer; watchdog-aware

**[PASS]**

- `src/ui/` contains no interrupt service routines.
- `touch_read_cb()` (`src/ui/touch_mapping.cpp:44-69`) is a polled LVGL input-device callback, not an ISR. It reads the pre-registered `XPT2046_Touchscreen` instance, maps coordinates, logs, and returns immediately. No `malloc`, `new`, `free`, or C++ container allocation occurs inside it.
- Display and input objects are created once at startup (`ui_create()` in `src/ui/ui.cpp:197-209` and the `create_*_screen()` functions in `src/ui/screens.cpp`). The display buffer is a static array (`static lv_color_t s_buf[DISP_WIDTH * 10];` at `src/ui/ui.cpp:39`), and the LVGL driver structs are `static` local objects.
- UI code does not access GPIO registers or raw pin numbers directly. All pin/display constants are referenced through `hal_config.h` (e.g., `TOUCH_MIN_X_RAW`, `DISP_WIDTH`). The touchscreen and display are accessed through their initialized driver classes.
- The UI layer is watchdog-aware: event handlers and the touch read callback return without blocking, and `ui_handler()` simply calls `lv_timer_handler()` so the main loop can also feed the watchdog.

### G10.4 — No blocking delays in display or touch input handling

**[PASS]**

- No `delay()`, `vTaskDelay()`, `micros()` polling loops, or busy-wait loops exist in `src/ui/`.
- LVGL event handlers return immediately after posting events (`src/ui/ui.cpp:85-160`).
- `touch_read_cb()` performs a single non-blocking `touched()` check, maps coordinates, and returns (`src/ui/touch_mapping.cpp:44-69`).
- `ui_handler()` and `ui_tick()` are thin wrappers around `lv_timer_handler()` and `lv_tick_inc()` with no added delays (`src/ui/ui.cpp:187-195`).

## Additional Checks (U01–U05)

- **[PASS] U01 — Gameplay scoreboard moved to bottom center; results "Play Again" button moved up.**  
  - `src/ui/screens.cpp:164` aligns `lblScore` on `SCR_Gameplay` with `LV_ALIGN_BOTTOM_MID, 0, -20`.  
  - `src/ui/screens.cpp:201` aligns the result-screen `lblScore` with `LV_ALIGN_BOTTOM_MID, 0, -20`.  
  - `src/ui/screens.cpp:215-218` creates `btnPlayAgain` at y = 200 px, moved up from the original 250 px to avoid overlap with the bottom-center scoreboard.

- **[PASS] U02 — All text is white; `#404040` is used only for backgrounds/borders.**  
  - `src/ui/theme.cpp:18` sets `text_secondary()` to white.  
  - `src/ui/theme.cpp:80` sets the error title text color to `text_primary()` (white).  
  - `src/ui/theme.cpp:111,120,130,146` set button, home-icon, score, and outcome label text colors to white.  
  - `src/ui/theme.cpp:102` uses `accent_grey()` (`#404040`) only for the `LV_STATE_PRESSED` button background, consistent with the bug-report allowance for backgrounds/borders.

- **[PASS] U03 — Host-timeout dialog buttons are narrower and no longer overlap.**  
  - `src/ui/screens.cpp:142-148` creates the dialog buttons with size 85 × 45 px and aligns them to `LV_ALIGN_BOTTOM_LEFT` and `LV_ALIGN_BOTTOM_RIGHT` with 15 px margins inside a 220 px wide panel, leaving clear separation.

- **[PASS] U04 — Host wait progress bar has blue fill and black background.**  
  - `src/ui/screens.cpp:103-104` sets the bar main background to `ui_theme::bg_black()`.  
  - `src/ui/screens.cpp:105-106` sets the bar indicator to `lv_color_hex(0x0066FF)` (blue).

- **[PASS] U05 — Results screen scoreboard reads the same live score as the gameplay screen.**  
  - `src/ui/ui.cpp:305-316` caches the last wins/losses/draws passed to `ui_set_score()`.  
  - `src/ui/ui.cpp:261-262` calls `ui_set_score(s_score_wins, s_score_losses, s_score_draws)` inside `ui_show_screen_result()`, ensuring the result label displays the same values that were last shown on the gameplay screen. The score source remains `ctx_.score` in `src/state_machine/app_state_machine.cpp:513-525`.

## Notes

- The UI fixes intentionally deviate from the older layout descriptions in `docs/definition.md` Section 2 (which still show the gameplay score at top-left and the result-screen "Play Again" button at y ≈ 250 px). The bug report U01/U05 take precedence for v0.2.1; the definition document should be updated in a follow-up documentation pass to avoid future drift.
- `ui_set_peer_device_id()` remains an optional no-op adapter; the contract marks peer-device-id display as optional, and the state machine surfaces peer status through `ui_set_status()` instead.
