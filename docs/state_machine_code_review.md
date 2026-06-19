# CYD_RPS Stage 12 — Firmware Logic Code Review

**Workflow ID:** wvc_20260619_134606  
**Project:** CYD_RPS  
**Version:** 0.1.7  
**Revision Type:** bug_fix  
**Reviewer:** Firmware Logic Code Critic (Stage 12)  
**Review Date:** 2026-06-19  

---

## Verdict: GO

All MAOA Stage 12 quality gates pass. The Stage 11 v0.1.7 changes correctly address the BLE multiplayer JOIN connection failure (`status=13` / `BLE_ERR_CONN_REJ_RESOURCES`) documented in `docs/BugReport_CYD_RPS_v0.1.6.md` §8:

1. The JOIN now stops advertising immediately before initiating the connection.
2. The per-attempt connect timeout is reduced from the 30 s NimBLE default to 5 s.
3. On retry exhaustion the JOIN restarts advertising and posts `EV_ERROR`.

The firmware compiles for both the physical hardware and Wokwi simulation environments.

---

## Build / Test Evidence

| Command | Result | Notes |
|---------|--------|-------|
| `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** | Firmware image built; RAM 23.2%, Flash 74.0%. |
| `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** | Wokwi image built with `-D WOKWI_SIMULATION`; RAM 20.9%, Flash 67.4%. |
| `pio check` | **SUCCESS — 0 high-severity issues** | No high-severity issues in project source. |
| `pio test` | **N/A — no test directory** | Project has no `test/` folder; PlatformIO reports `TestDirNotExistsError`. This is expected per the task qualifier "if tests exist". |

---

## Quality Gate Results

### G12.1 — Every transition in `state_machine.puml` has a corresponding code path
**PASS**

`src/state_machine/state_machine_generated.cpp` implements every transition shown in `docs/state_machine.puml`. No states, events, guards, or actions were changed in this revision, so the prior transition-by-transition mapping remains intact.

### G12.2 — Guards reference `dependency_manifest.json` facts or runtime probes, not hardcoded assumptions
**PASS**

All guards in `app_state_machine.cpp` read runtime state:

- `guard_init_ok` / `guard_ble_init_failed` / `guard_hw_init_failed` / `guard_fatal` probe `hal_service()`.
- `guard_game_mode_eq_*` read `ctx_.game_mode`.
- `guard_local_move_chosen` / `guard_peer_move_received` families read `ctx_.local_move` / `ctx_.peer_move`.
- `guard_role_host` / `guard_role_join` read `ble_service().role()`.

No guard contains hardcoded constants or assumptions about the runtime environment.

### G12.3 — No LVGL or UI toolkit imports in state machine code
**PASS**

The reviewed state-machine files (`ble_service.h`, `ble_service.cpp`, `state_machine_generated.cpp`, `state_machine_generated.h`, `app_state_machine.cpp`, `app_state_machine.h`, `game_logic.h`, `hal_service.h`) contain no `#include <lvgl.h>`, LVGL types, or UI toolkit symbols. `app_state_machine.h` declares only weak UI adapter callbacks with primitive C types.

### G12.4 — All external calls have defined error-state transitions; no silent exception swallowing
**PASS**

- `BleService::init()` returns a Boolean; `main.cpp` converts a `false` result into `hal.mark_ble_init_failed(true)`, which `app_init()` maps to `EV_ERROR` and the PUML `Boot --> Error` transition.
- `do_start_discovery()` posts `EV_ERROR` if `start_scanning()` fails.
- `start_advertising_if_needed()` and `become_host()` post `EV_ERROR` if advertising cannot start.
- `connect_to_host()` posts `EV_ERROR` on invalid role/state, client creation failure, exhausted retries, service/characteristic discovery failure, and subscription failure.
- `send_move()` returns `false` on failure; every caller (`on_local_move_*_complete/pending`) explicitly invokes `app_on_error("Failed to send move")` to post `EV_ERROR`.
- `BleService::on_peer_found()` ignores invalid/self advertisements safely but does not swallow operational errors.

No external-call failure path silently drops the error.

### G12.5 — Generated code matches `state_machine.puml` transition-for-transition
**PASS**

`state_machine_generated.cpp` was compared line-by-line against `docs/state_machine.puml`. All states, events, guard predicates, and action calls match. The generated code uses the same state ordering and transition targets as the diagram. The v0.1.7 fix did not modify the PUML or the generated dispatcher.

### G12.6 — No blocking delays in state-machine update loops
**PASS**

- `AppStateMachine::update()` only calls `ble_service().update(millis())`, `update_search_timeout_display()`, and `dispatch_pending()` — all non-blocking.
- `BleService::update()` only calls `post_peer_timeout_if_needed()`, which is a simple timestamp comparison.
- The 250 ms retry backoff and the 5 s `NimBLEClient::connect()` timeout live inside the dedicated `ble_radio` task loop (`radio_task_loop()`), not the state-machine update path. The backoff uses `vTaskDelay`, which yields the radio task.
- The `delay(5)` in `main.cpp` `loop()` is outside the state-machine update function.

### G12.7 — All GPIO access goes through the HAL/pins.h layer; ISR and watchdog safe
**PASS**

No state-machine source file performs direct GPIO register access. `main.cpp` owns the one-time GPIO setup (backlight, SPI, touch) using `pins.h` constants. The state machine layer only probes `HalService` flags and delegates BLE operations to the radio task.

---

## Stage 11 / v0.1.7 Specific Findings

### 1. Radio-task ownership
**PASS**

`BleService::connect_to_host()` is only invoked from the dedicated radio task, inside the `RadioCommand::CONNECT_TO_HOST` handler of `radio_task_loop()` (`ble_service.cpp` lines 592–596). The state-machine action `start_scanning_join()` posts the command via `signal_connect_to_host()` (`app_state_machine.cpp` lines 292–299), which uses `xQueueSend` to transfer work to the radio task. No NimBLE advertise/connect work is initiated from a state-machine action on the main/LVGL stack.

### 2. Address-type preservation
**PASS**

`RpsAdvertisedDeviceCallbacks::onResult()` passes `advertisedDevice->getAddress().getType()` to `on_peer_found()` (`ble_service.cpp` lines 78). The type is stored in `peer_addr_type_` (line 342) and reset at the start of each discovery cycle (line 229). `connect_to_host()` constructs `NimBLEAddress(addr_buf, peer_addr_type_)` (line 440), preserving the discovered type through the connection. This is the v0.1.4 fix (LL-045) and has not regressed.

### 3. JOIN advertising timing
**PASS**

`connect_to_host()` calls `stop_advertising()` at line 433 before it calls `cli->connect(addr)` at line 457. The in-file comment at lines 428–432 explicitly cites `docs/BugReport_CYD_RPS_v0.1.6.md` §8.5 and explains that continuing to advertise while initiating a connection causes the ESP32 NimBLE controller to reject the attempt with `status=13` (`BLE_ERR_CONN_REJ_RESOURCES`).

`become_join()` still keeps advertising during role negotiation (lines 399–412), satisfying LL-046; the advertising is only stopped at the moment the JOIN transitions from "negotiation" to "connecting" inside the radio task.

### 4. Connect timeout
**PASS**

`BleService::kConnectTimeoutSeconds` is declared in `ble_service.h` at line 65 with a rationale comment referencing `docs/BugReport_CYD_RPS_v0.1.6.md` §8.6. In `connect_to_host()`, `cli->setConnectTimeout(kConnectTimeoutSeconds)` is called at line 446, before the `for` loop that performs the retries (lines 453–462). This reduces the per-attempt timeout from the NimBLE default of 30 s to 5 s, matching the bug-report recommendation.

### 5. Failure cleanup
**PASS**

When all retries are exhausted, `connect_to_host()` (lines 464–473):

1. Logs the failure.
2. Calls `start_advertising()` to make the JOIN discoverable again (line 469).
3. Deletes the client and clears `client_` (lines 470–471).
4. Posts `Event::EV_ERROR` (line 472).

This satisfies the bug-report §8.6 recommendation to restart advertising on connection failure before reporting the error.

### 6. No regressions
**PASS**

The v0.1.2–v0.1.6 fixes are still present:

| Fix | Location | Status |
|-----|----------|--------|
| GATT server started during init (v0.1.2/LL-044) | `ble_service.cpp` lines 117–146 | Present — `server_ptr(server_)->start()` is called before any GAP procedure. |
| Non-blocking scan (v0.1.1/LL-036) | `ble_service.cpp` line 660 | Present — `scan_ptr(scan_)->start(0, nullptr, false)`. |
| Wokwi fallback (v0.1.1/LL-037) | `ble_service.cpp` lines 237–243 | Present — forces discovery timeout immediately under `WOKWI_SIMULATION`. |
| Thread-safe event queue (v0.1.2/LL-040) | `app_state_machine.cpp` lines 79–150 | Present — `queue_mutex_` protects `enqueue()` and `dispatch_pending()`. |
| Public-MAC manufacturer data (v0.1.6/LL-047) | `ble_service.cpp` lines 679–688; `ble_service.h` lines 67–70, 209–214 | Present — local public MAC embedded in manufacturer data and parsed in `onResult()`. |
| Bounded retry loop (v0.1.6/LL-046) | `ble_service.cpp` lines 452–462 | Present — up to `kConnectRetries` (4) attempts with `kConnectRetryDelayMs` (250 ms) backoff. |

### 7. In-file documentation (LL-043)
**PASS**

The v0.1.7 bug-report-driven change has an adjacent rationale comment:

| Change | File | Comment location |
|--------|------|------------------|
| Connect timeout reduction | `ble_service.h` | `kConnectTimeoutSeconds` lines 60–65 |
| Stop JOIN advertising before connect | `ble_service.cpp` | `connect_to_host()` lines 428–432 |
| Restart advertising on connect failure | `ble_service.cpp` | `connect_to_host()` lines 466–468 |

All comments state what changed, why, and point to the relevant bug-report section, satisfying LL-043.

---

## Defects / Action Items

None. All Stage 12 quality gates pass and the v0.1.7 fix is ready for the next workflow stage.

---

## Summary

The Stage 11 v0.1.7 revision correctly fixes the BLE multiplayer JOIN connection failure described in `docs/BugReport_CYD_RPS_v0.1.6.md` §8. The code is well-documented, compiles for both target environments, passes `pio check` with zero high-severity issues, and satisfies all Stage 12 quality gates. No regressions were identified in the prior v0.1.2–v0.1.6 fixes.

**Recommended action:** Proceed to the next workflow stage.
