# State Machine Review: CYD_RPS

**Project:** CYD_RPS  
**Version:** 0.1.0  
**Workflow ID:** wvc_20260617_044037  
**Board:** CYD2USB_v3  
**Target Environment:** wokwi  
**Review Stage:** 6 — State Machine Review Agent  

**Verdict:** GO

---

## Summary

The PlantUML state machine in `docs/state_machine.puml` covers the full Rock-Paper-Scissors flow described in `docs/definition.md`, including boot, peer discovery, role negotiation, single-player fallback, move selection, evaluation, result display, disconnect handling, and fatal/non-fatal error paths. The UI-to-state contract in `docs/state_machine_contract.json` binds every control listed in the UI/Display Layout section of `definition.md` to a state-machine action or transition.

The canonical generator `execution/generate_state_machine_cpp.py` was executed against `docs/state_machine.puml` and produced valid C++ source and header files without warnings, confirming generator compatibility.

---

## Findings

### G6.1 — Spec completeness
**[PASS]** Every functional requirement in `definition.md` is represented by at least one state, transition, or action:

- Boot/init success and init failures → `Boot`, `Boot --> Start [init_ok]`, `Boot --> Error [ble_init_failed]`, `Boot --> Halted [hw_init_failed]`.
- One-tap peer search → `Start --> PeerSearch : EV_PLAY / on_play_button()`.
- Cancel search → `PeerSearch --> Start : EV_CANCEL / on_cancel_button()`.
- Peer discovery and role negotiation → `PeerSearch --> RoleNegotiating`, `RoleNegotiating --> Connecting [role_host|role_join]`.
- Peer connected → `Connecting --> Gameplay : EV_CONNECTED`.
- Discovery timeout / single-player fallback → `PeerSearch --> SinglePlayerFallback : EV_PEER_TIMEOUT`.
- Rock/Paper/Scissors selection → `Gameplay` and `SinglePlayerFallback` move transitions.
- Peer move receive (both before and after local move) → `Gameplay --> Evaluating [local_move_chosen]` and `Gameplay --> Gameplay [!local_move_chosen]`.
- Round evaluation and result display → `Evaluating --> Result : EV_EVALUATE / evaluate_and_show_result()`.
- New round → `Result --> Gameplay [MODE_MULTI_PLAYER]` and `Result --> SinglePlayerFallback [MODE_SINGLE_PLAYER]`.
- Peer disconnect → `Connecting --> Disconnected`, `Gameplay --> Disconnected`, `Result --> Disconnected`.
- Retry from error → `Error --> Start : EV_RETRY`, `Disconnected --> Start : EV_RETRY`.

### G6.2 — Contract completeness
**[PASS]** Every UI control described in the UI/Display Layout section of `definition.md` is bound in `state_machine_contract.json`:

- `SCR_Start`: `lblTitle`, `lblSubtitle`, `btnPlay`, `lblStatusStart`.
- `SCR_PeerSearch`: `lblTitle`, `lblStatusSearch`, `barSearch`, `lblTimeout`, `btnCancel`.
- `SCR_Gameplay`: `lblTitle`, `lblStatusGame`, `btnRock`, `btnPaper`, `btnScissors`.
- `SCR_Result`: `lblTitle`, `lblLocalMove`, `lblPeerMove`, `lblOutcome`, `btnPlayAgain`.
- `SCR_Error`: `lblErrorTitle`, `lblErrorMsg`, `btnRetry`.

All touch-enabled controls post events that drive transitions; all non-touch controls are bound to state-entry or timer-driven update actions.

### G6.3 — Testability
**[PASS]** Every transition can be expressed as `Starting State -> Action -> Expected Result` with a measurable observable:

- State changes are observable via `StateMachine::current()` in generated code.
- Actions are discrete virtual hooks (e.g., `on_play_button()`, `start_advertising_host()`, `evaluate_and_show_result()`).
- UI updates, serial log strings, and BLE procedure calls provide measurable side effects for each transition.
- Self-loop transitions (e.g., `Gameplay --> Gameplay` while waiting for the peer move) keep the same state but produce observable status/button-state changes.

### G6.4 — Error coverage for unavailable external dependencies
**[PASS]** All runtime dependencies marked `present: false` in `dependency_manifest.json` have defined error/unavailable paths:

- CYD2USB physical board, ST7789 display, and XPT2046 touchscreen unavailable at runtime → `Boot --> Halted : EV_ERROR [hw_init_failed] / app_on_error_hw()`.
- ESP32 integrated BLE radio unavailable → `Boot --> Error : EV_ERROR [ble_init_failed] / app_on_error_ble()`.
- The missing preferred PlatformIO board package (`esp32-2432s028r_cyd2usb`) is a build-time dependency; the manifest records a functional `esp32dev` fallback, so it does not require a runtime state-machine path.

### G6.5 — Guard consistency
**[PASS]** Guards reference only runtime-discoverable facts or internal game/BLE state:

- `init_ok`, `ble_init_failed`, `hw_init_failed`, `fatal` — runtime initialization outcomes.
- `role_host`, `role_join` — derived from public BLE MAC comparison at runtime.
- `peer_move_received`, `local_move_chosen`, `game_mode == MODE_*` — internal game state.

No guard references a UI widget object or a dependency-manifest static flag directly.

### G6.6 — Hardware error coverage
**[PASS]** Every hardware dependency listed in `dependency_manifest.json` has an error-state path:

- Display/touch/board hardware → `Boot --> Halted` via `[hw_init_failed]`.
- BLE radio → `Boot --> Error` via `[ble_init_failed]`.
- Runtime BLE link loss → `Connecting --> Disconnected`, `Gameplay --> Disconnected`, `Result --> Disconnected`.

### G6.7 — Guards do not hardcode pin numbers
**[PASS]** No guard expression contains a GPIO number or hardware pin constant. All guards are logical/state-based.

### G6.8 — PlantUML generator compatibility
**[PASS]** `execution/generate_state_machine_cpp.py` successfully parsed `docs/state_machine.puml` and emitted valid C++:

- Command: `python execution/generate_state_machine_cpp.py --input projects/CYD_RPS/docs/state_machine.puml --output .tmp/CYD_RPS_state_machine_generated.cpp --namespace app`
- Result: generated `.tmp/CYD_RPS_state_machine_generated.cpp` and `.tmp/CYD_RPS_state_machine_generated.h` with no warnings.
- The diagram uses only supported features: simple `state Name` declarations, `[*] --> Boot` initial pseudo-state, and `Source --> Target : event [guard] / action` transitions.
- No composite states, history states, forks/joins, or terminal `[*]` targets are present.

---

## Gaps

None identified. All quality gates pass.
