# State Machine Review: CYD_RPS

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Board:** CYD2USB_v3  
**Target Environment:** hardware (Wokwi non-default)  
**Review Stage:** 6 — State Machine Review Agent  

**Verdict:** GO

---

## Summary

The PlantUML state machine in `docs/state_machine.puml` covers the full v0.2.0 Rock-Paper-Scissors flow described in `docs/definition.md`, including boot/init success and failure paths, explicit Host selection, peer auto-join, Host timeout dialog with Retry/Solo choices, single-player fallback, move selection for both multiplayer and single-player modes, peer move reception, round evaluation, result display, peer disconnect handling, and fatal/non-fatal error paths.

The UI-to-state contract in `docs/state_machine_contract.json` binds every control listed in the UI/Display Layout section of `definition.md` to a state-machine action or transition, including the new HostWait screen, Host timeout dialog, and updated Start screen controls.

The canonical generator `execution/generate_state_machine_cpp.py` was executed against `docs/state_machine.puml` and produced valid C++ source and header files without warnings, confirming generator compatibility.

---

## Findings

### G6.1 — Spec completeness
**[PASS]** Every functional requirement in `definition.md` is represented by at least one state, transition, or action:

- Boot/init success and init failures → `Boot`, `Boot --> Start [init_ok]`, `Boot --> Error [ble_init_failed]`, `Boot --> Halted [hw_init_failed]`.
- Explicit Host selection by tapping **Host Game** or serial `HOST` → `Start --> HostWait : EV_HOST_GAME / on_host_game()`.
- Peer auto-join via presence beacon → `Start --> Joining : EV_HOST_FOUND [host_mac_valid] / on_join_detected()`.
- One-tap **Solo** flow → `Start --> Gameplay : EV_SOLO / on_solo()` and `HostTimeoutDialog --> Gameplay : EV_SOLO / on_host_solo()`.
- Host timeout dialog with **Retry** and **Solo** → `HostWait --> HostTimeoutDialog : EV_HOST_TIMEOUT / on_host_timeout()`.
- Cancel hosting → `HostWait --> Start : EV_CANCEL_HOST / on_cancel_host()` (also available from `HostTimeoutDialog`).
- Simultaneous Host conflict resolution → `HostWait --> Joining : EV_HOST_FOUND [peer_host_seen] / on_conflict_become_join()`.
- Successful peer connection → `HostWait --> Gameplay : EV_CONNECTED / on_peer_connected()` and `Joining --> Gameplay : EV_CONNECTED / on_peer_connected()`.
- Join connection failure / retries exhausted → `Joining --> Start : EV_CONNECT_FAILED [retries_exhausted] / on_join_failed()`.
- Rock/Paper/Scissors selection (touch or serial) → `Gameplay` move transitions for both `mode_single` and `mode_multi`.
- Peer move receive before local move → `Gameplay --> Gameplay : EV_PEER_MOVE_RECEIVED [!local_move_chosen] / on_peer_move_pending()`.
- Peer move receive after local move → `Gameplay --> Evaluating : EV_PEER_MOVE_RECEIVED [local_move_chosen] / on_peer_move_complete()`.
- Single-player opponent move generation → `Gameplay --> Evaluating : EV_MOVE_* [mode_single] / on_singleplayer_move_*()`.
- Round evaluation and result display → `Evaluating --> Result : EV_EVALUATE / evaluate_and_show_result()`.
- Session scoreboard update → `ui_set_score()` bound in contract, called on gameplay entry and after evaluation.
- Repeatable new round → `Result --> Gameplay : EV_PLAY_AGAIN / start_new_round()`.
- Global firmware reset without power cycle → `EV_RESET` transitions to `Halted` via `esp_restart()` from every non-Boot/Joining state.
- Hardware testing via debug serial command parser → serial command bindings in `state_machine_contract.json` map `HOST`, `SOLO`, `ROCK`, `PAPER`, `SCISSORS`, `AGAIN`, `RESET`, and `HOME` to equivalent state-machine events.

### G6.2 — Contract completeness
**[PASS]** Every UI control described in the UI/Display Layout section of `definition.md` is bound in `state_machine_contract.json`:

- `SCR_Start`: `lblTitle`, `lblDeviceId`, `lblStatus`, `btnHostGame`, `btnSolo`, `btnHome`.
- `SCR_HostWait`: `lblTitle`, `lblDeviceId`, `lblStatus`, `barProgress`, `btnCancel`, `btnHome`.
- `HostTimeoutDialog`: `btnRetry`, `btnSolo`.
- `SCR_Gameplay`: `lblTitle`, `lblScore`, `lblStatus`, `btnRock`, `btnPaper`, `btnScissors`, `btnHome`.
- `SCR_Result`: `lblTitle`, `lblScore`, `lblLocalMove`, `lblPeerMove`, `lblOutcome`, `btnPlayAgain`, `btnHome`.
- `SCR_Error`: `lblErrorTitle`, `lblErrorMsg`, `btnRetry`, `btnHome`.

All touch-enabled controls post events that drive transitions; all non-touch controls are bound to state-entry, timer-driven, or BLE-callback update actions.

### G6.3 — Testability
**[PASS]** Every transition can be expressed as `Starting State -> Action -> Expected Result` with a measurable observable:

- State changes are observable via `StateMachine::current()` in generated code.
- Actions are discrete virtual hooks (e.g., `on_host_game()`, `on_peer_connected()`, `evaluate_and_show_result()`).
- UI updates, serial log strings, BLE procedure calls, and scoreboard changes provide measurable side effects for each transition.
- Self-loop transitions (e.g., `Gameplay --> Gameplay` while waiting for the peer move) keep the same state but produce observable status/button-state changes.

### G6.4 — Error coverage for unavailable external dependencies
**[PASS]** All runtime dependencies marked `present: false` in `dependency_manifest.json` have defined error/unavailable paths:

- CYD2USB physical board, ST7789 display, and XPT2046 touchscreen unavailable at runtime → `Boot --> Halted : EV_ERROR [hw_init_failed] / app_on_error_hw()`.
- ESP32 integrated BLE radio unavailable → `Boot --> Error : EV_ERROR [ble_init_failed] / app_on_error_ble()`.
- The missing preferred PlatformIO board package (`esp32-2432s028r_cyd2usb`) is a build-time dependency; the manifest records a functional `esp32dev` fallback, so it does not require a runtime state-machine path.

### G6.5 — Guard consistency
**[PASS]** Guards reference only runtime-discoverable facts or internal game/BLE state:

- `init_ok`, `ble_init_failed`, `hw_init_failed`, `fatal` — runtime initialization outcomes.
- `host_mac_valid`, `peer_host_seen` — derived from BLE scan events at runtime.
- `mode_single`, `mode_multi`, `peer_move_received`, `local_move_chosen`, `retries_exhausted` — internal game/BLE state.

No guard references a UI widget object or a dependency-manifest static flag directly.

### G6.6 — Hardware error coverage
**[PASS]** Every hardware dependency listed in `dependency_manifest.json` has an error-state path:

- Display/touch/board hardware → `Boot --> Halted` via `[hw_init_failed]`.
- BLE radio → `Boot --> Error` via `[ble_init_failed]`.
- Runtime BLE link loss → `Gameplay --> Disconnected` and `Result --> Disconnected` via `EV_DISCONNECTED`.

### G6.7 — Guards do not hardcode pin numbers
**[PASS]** No guard expression contains a GPIO number or hardware pin constant. All guards are logical/state-based.

### G6.8 — PlantUML generator compatibility
**[PASS]** `execution/generate_state_machine_cpp.py` successfully parsed `docs/state_machine.puml` and emitted valid C++:

- Command: `python execution/generate_state_machine_cpp.py --input projects/CYD_RPS/docs/state_machine.puml --output .tmp/CYD_RPS_state_machine_generated_v0.2.0_check.cpp --namespace app`
- Result: generated `.tmp/CYD_RPS_state_machine_generated_v0.2.0_check.cpp` and `.tmp/CYD_RPS_state_machine_generated_v0.2.0_check.h` with no warnings.
- The diagram uses only supported features: simple `state Name` declarations, `[*] --> Boot` initial pseudo-state, and `Source --> Target : event [guard] / action` transitions.
- No composite states, history states, forks/joins, or terminal `[*]` targets are present.

---

## Gaps

None identified. All quality gates pass.
