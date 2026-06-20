# CYD_RPS v0.1.8 — Firmware Logic Code Review (Stage 12)

| Field | Value |
|-------|-------|
| **Workflow ID** | wvc_20260619_072600 |
| **Project** | CYD_RPS |
| **Version** | 0.1.8 |
| **Revision Type** | bug_fix |
| **Reviewer** | Firmware Logic Code Critic (Stage 12) |
| **Date** | 2026-06-20 |

---

## Verdict: **GO**

The v0.1.8 BLE discovery-race fix compiles cleanly for both target environments and satisfies all Stage 12 quality gates. The JOIN now keeps advertising for a bounded discovery window before each `connect()` attempt, and the HOST has repeated opportunities to discover the JOIN and stop scanning before the connection is initiated.

---

## Build Verification

| Environment | Command | Result |
|-------------|---------|--------|
| Physical CYD2USB | `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** (Flash: 970,205 bytes / 1,310,720; RAM: 76,036 bytes / 327,680) |
| Wokwi simulation | `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** (Flash: 883,321 bytes / 1,310,720; RAM: 68,648 bytes / 327,680) |

Both builds completed without errors or warnings that would block the review.

---

## Quality Gate Findings

### G12.1 — Every transition in `state_machine.puml` has a corresponding code path

**Status: PASS**

All transitions defined in `docs/state_machine.puml` are present in `src/state_machine/state_machine_generated.cpp`:

- `Boot --> Start`, `Boot --> Error`, `Boot --> Halted`
- `Start --> PeerSearch`, `Start --> Error`
- `PeerSearch --> RoleNegotiating`, `PeerSearch --> SinglePlayerFallback`, `PeerSearch --> Start`, `PeerSearch --> Error`
- `RoleNegotiating --> Connecting` (host and join guards), `RoleNegotiating --> Error`
- `Connecting --> Gameplay`, `Connecting --> Disconnected`, `Connecting --> Error`
- All `Gameplay` move transitions and `EV_PEER_MOVE_RECEIVED` transitions
- `SinglePlayerFallback --> Evaluating`, `SinglePlayerFallback --> Error`
- `Evaluating --> Result`, `Evaluating --> Error`
- `Result --> Gameplay`, `Result --> SinglePlayerFallback`, `Result --> Disconnected`, `Result --> Error`
- `Disconnected --> Start`, `Disconnected --> Error`
- `Error --> Start`, `Error --> Halted`

No PUML transition is missing or unreachable.

### G12.2 — Guards reference dependency-manifest facts or runtime probes, not hardcoded assumptions

**Status: PASS**

All guards in `app_state_machine.cpp` read runtime state:

- `guard_init_ok`, `guard_ble_init_failed`, `guard_hw_init_failed`, `guard_fatal` → `hal_service()` probes.
- `guard_game_mode_eq_MODE_MULTI_PLAYER` / `MODE_SINGLE_PLAYER` → `ctx_.game_mode`.
- `guard_local_move_chosen` / `guard_not_local_move_chosen` → `ctx_.local_move`.
- `guard_peer_move_received` / `guard_not_peer_move_received` → `ctx_.peer_move`.
- `guard_role_host` / `guard_role_join` → `ble_service().role()`.

No guard contains a hardcoded constant or assumption about the runtime environment.

### G12.3 — No LVGL or UI toolkit imports in state machine code

**Status: PASS**

- `app_state_machine.cpp` only declares weak UI adapter stubs (`ui_show_screen_*`, `ui_set_*`); it does **not** include `lvgl.h` or any LVGL types.
- `ble_service.cpp` / `ble_service.h` include only `Arduino.h`, `NimBLEDevice.h`, FreeRTOS headers, and project headers.
- `state_machine_generated.cpp` has no UI imports.
- Grep confirms no `lvgl`/`LVGL` imports in `src/state_machine`; references are confined to comments.

### G12.4 — All external calls have defined error-state transitions; no silent exception swallowing

**Status: PASS**

No `try`/`catch` blocks are used in the state-machine code, and no `delay()` calls are present. Error paths route through `EV_ERROR` or `EV_DISCONNECTED` as defined in the PUML:

- `do_start_discovery()` posts `EV_ERROR` when NimBLE is uninitialized or `start_scanning()` fails.
- `start_advertising_if_needed()` posts `EV_ERROR` when advertising cannot start.
- `become_host()` posts `EV_ERROR` when `start_advertising()` fails.
- `become_join()` stops scanning but keeps advertising and posts `EV_ROLE_RESOLVED`; advertising failure is not applicable here because it was already started during discovery.
- `connect_to_host()` posts `EV_ERROR` on client creation failure, every connect retry failure, service/characteristic discovery failure, and subscription failure.
- `send_move()` returns a boolean; callers in `app_state_machine.cpp` check the return value and call `app_on_error()` on failure.

No external call silently swallows a failure or stalls without a PUML-defined exit.

### G12.5 — Generated code matches `state_machine.puml` transition-for-transition

**Status: PASS**

`src/state_machine/state_machine_generated.cpp` was generated from `docs/state_machine.puml` and the dispatch table is a faithful, one-to-one mapping:

| PUML Transition | Generated Handler |
|-----------------|-------------------|
| `Boot --> Start : EV_BOOT_DONE [init_ok] / app_init_success()` | `case State::Boot: if (event == EV_BOOT_DONE && guard_init_ok(ctx)) { app_init_success(); state_ = State::Start; }` |
| `RoleNegotiating --> Connecting : EV_ROLE_RESOLVED [role_join] / start_scanning_join()` | `case State::RoleNegotiating: if (event == EV_ROLE_RESOLVED && guard_role_join(ctx)) { start_scanning_join(); state_ = State::Connecting; }` |
| `Gameplay --> Evaluating : EV_MOVE_ROCK [peer_move_received] / on_local_move_rock_complete()` | `case State::Gameplay: if (event == EV_MOVE_ROCK && guard_peer_move_received(ctx)) { on_local_move_rock_complete(); state_ = State::Evaluating; }` |

Every event/guard/action tuple from the PUML appears in the generated dispatcher with the correct target state. No spurious transitions were introduced.

### G12.6 — No blocking delays in state-machine update loops

**Status: PASS**

- `AppStateMachine::update()` is purely non-blocking: it calls `ble_service().update(millis())`, dispatches deferred discovery, updates the UI label, and drains the event queue.
- The only blocking waits are `vTaskDelay()` inside `BleService::connect_to_host()`, which runs on the dedicated radio task (core 0), not the Arduino main loop or LVGL dispatch context.
- Grep confirms no `delay()` calls in `src/state_machine`.

---

## Additional BLE-Specific Checks

| Check | Finding | Status |
|-------|---------|--------|
| JOIN does not stop advertising until immediately before `connect()` | `connect_to_host()` keeps advertising during each discovery window and calls `stop_advertising()` only immediately before `cli->connect(addr)`. | PASS |
| JOIN waits a bounded discovery window before first `connect()` and between retries | `kJoinDiscoveryWindowMs = 2000` is used before attempt 1; `kJoinConnectInterRetryWindowMs = 2000` is used before attempts 2–4. | PASS |
| Connect timeout is sized for the retry strategy (5 s per attempt) | `kConnectTimeoutSeconds = 5` is passed to `cli->setConnectTimeout()`. With 4 retries and 2 s windows, total negotiation time stays bounded under ~30 s. | PASS |
| Peer address type captured during scanning is used verbatim in `NimBLEClient::connect()` | `on_peer_found()` stores `peer_addr_type_`; `connect_to_host()` constructs `NimBLEAddress addr(addr_buf, peer_addr_type_)`. | PASS |
| All NimBLE scan/advertise/connect operations are owned by the dedicated radio task | `signal_discovery_start()`, `signal_become_host()`, and `signal_connect_to_host()` enqueue commands consumed by `radio_task_loop()`. All NimBLE GAP/GATT work executes on that task stack. | PASS |
| Bug-report-driven timing constants have adjacent rationale comments referencing `docs/BugReport_CYD_RPS_v0.1.7.md` | `kConnectTimeoutSeconds`, `kJoinDiscoveryWindowMs`, `kJoinConnectInterRetryWindowMs`, and `kPeerSearchAdvMinInterval`/`kPeerSearchAdvMaxInterval` all cite the bug report in their header comments. Inline comments in `connect_to_host()` also cite §9.1, §9.2, and §11.2. | PASS |

---

## Observations (Non-Blocking)

1. **Action naming:** `AppStateMachine::start_scanning_join()` is named after the PUML action but, for the JOIN role, it actually initiates the central connection via `signal_connect_to_host()`. The behavior is correct; the name reflects the contract document.
2. **`signal_become_host()` naming:** The method name suggests HOST-only resolution, but it calls `resolve_role()`, which may resolve either HOST or JOIN. The implementation is correct and consistent with the PUML.
3. **HOST path advertising interval:** When the HOST stops scanning and continues advertising, `start_advertising()` uses the normal relaxed interval (`0x800` units = 1.28 s). This is acceptable because the JOIN has already discovered the HOST; the HOST only needs to remain connectable.

---

## Conclusion

The v0.1.8 BLE discovery-window fix is **ready to proceed**. It addresses the root cause identified in `docs/BugReport_CYD_RPS_v0.1.7.md` (the JOIN stopping advertising before the HOST discovers it), keeps all radio work on the dedicated task, preserves the existing state-machine contract, and compiles for both the physical hardware and Wokwi environments.

**Final Verdict: GO**
