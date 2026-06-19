# CYD_RPS Stage 12 — Firmware Logic Code Review

**Workflow ID:** wvc_20260619_034654  
**Project:** CYD_RPS  
**Version:** 0.1.6  
**Revision Type:** bug_fix  
**Reviewer:** Firmware Logic Code Critic (Stage 12)  
**Review Date:** 2026-06-19  

---

## Verdict: GO

All MAOA Stage 12 quality gates pass. The Stage 11 v0.1.6 changes correctly address the three root causes documented in `docs/BugReport_CYD_RPS_v0.1.5.md`:

1. JOIN keeps advertising during role negotiation.
2. JOIN retries the HOST connection with a bounded backoff.
3. Role resolution uses the stable public MAC carried in the advertisement payload.

The firmware compiles for both the physical hardware and Wokwi simulation environments.

---

## Build / Test Evidence

| Command | Result | Notes |
|---------|--------|-------|
| `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** | Firmware image built; RAM 23.2%, Flash 74.0%. |
| `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** | Wokwi image built with `-D WOKWI_SIMULATION`; RAM 20.9%, Flash 67.4%. |
| `pio test` | **N/A — no test directory** | Project has no `test/` folder; PlatformIO reports `TestDirNotExistsError`. This is expected per the task qualifier "if tests exist". |

---

## Quality Gate Results

### G12.1 — Every transition in `state_machine.puml` has a corresponding code path
**PASS**

`src/state_machine/state_machine_generated.cpp` implements every transition shown in `docs/state_machine.puml`. A transition-by-transition comparison confirms all 40 named transitions (Boot/Start/PeerSearch/RoleNegotiating/Connecting/Gameplay/SinglePlayerFallback/Evaluating/Result/Disconnected/Error/Halted) are present with the correct events, guards, and actions.

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
- `connect_to_host()` posts `EV_ERROR` only after all retries are exhausted, and also on client creation, service discovery, characteristic discovery, or subscription failure.
- `send_move()` returns `false` on failure; every caller (`on_local_move_*_complete/pending`) explicitly invokes `app_on_error("Failed to send move")` to post `EV_ERROR`.
- `BleService::on_peer_found()` ignores invalid/self advertisements safely but does not swallow operational errors.

No external-call failure path silently drops the error.

### G12.5 — Generated code matches `state_machine.puml` transition-for-transition
**PASS**

`state_machine_generated.cpp` was compared line-by-line against `docs/state_machine.puml`. All states, events, guard predicates, and action calls match. The generated code uses the same state ordering and transition targets as the diagram.

### G12.6 — No blocking delays in state-machine update loops
**PASS**

- `AppStateMachine::update()` only calls `ble_service().update(millis())`, `update_search_timeout_display()`, and `dispatch_pending()` — all non-blocking.
- `BleService::update()` only calls `post_peer_timeout_if_needed()`, which is a simple timestamp comparison.
- The 250 ms backoff in `connect_to_host()` lives inside the dedicated `ble_radio` task loop (`radio_task_loop()`), not the state-machine update path. It uses `vTaskDelay`, which yields the radio task.
- The `delay(5)` in `main.cpp` `loop()` is outside the state-machine update function.

### G12.7 — All GPIO access goes through the HAL/pins.h layer; ISR and watchdog safe
**PASS**

No state-machine source file performs direct GPIO register access. `main.cpp` owns the one-time GPIO setup (backlight, SPI, touch) using `pins.h` constants. The state machine layer only probes `HalService` flags and delegates BLE operations to the radio task.

---

## Stage 11 / v0.1.6 Specific Findings

### 1. Role resolution correctness
**PASS**

`BleService::resolve_role()` (lines 350–378 of `ble_service.cpp`) uses `peer_public_addr_` when `peer_public_addr_valid_` is true, otherwise falls back to `peer_addr_`. The comparison uses `memcmp(local, peer_id, 6)`. A `cmp == 0` guard returns safely. Lower MAC becomes HOST, higher MAC becomes JOIN, exactly as the bug report §8.3 prescribes.

The self-discovery guard in `on_peer_found()` (lines 322–339) prefers the public MAC and falls back to the random advertisement address, preventing a unit from resolving against itself.

### 2. JOIN keeps advertising
**PASS**

`BleService::become_join()` (lines 399–412) calls `stop_scanning()` and deliberately does **not** call `stop_advertising()`. The in-file comment explicitly cites `docs/BugReport_CYD_RPS_v0.1.5.md` §6.1 and §8.1.

### 3. HOST advertising stability
**PASS**

`BleService::become_host()` (lines 380–397) calls `start_advertising()`, which checks `adv->isAdvertising()` and returns `true` immediately without restarting. This avoids regenerating the random address that the JOIN captured. Comment cites `docs/BugReport_CYD_RPS_v0.1.5.md` §6.4 and §8.4.

### 4. Connection retry safety
**PASS**

`BleService::connect_to_host()` (lines 414–487) loops up to `kConnectRetries` (4) with `kConnectRetryDelayMs` (250 ms) between attempts. The loop runs inside the `CONNECT_TO_HOST` command handler of `radio_task_loop()`. `EV_ERROR` is posted only after the loop exhausts all attempts. Comment cites `docs/BugReport_CYD_RPS_v0.1.5.md` §6.2 and §8.2.

### 5. Address-type preservation
**PASS**

`RpsAdvertisedDeviceCallbacks::onResult()` passes `advertisedDevice->getAddress().getType()` to `on_peer_found()`. `connect_to_host()` constructs `NimBLEAddress(addr_buf, peer_addr_type_)`, preserving the discovered type through the connection. This is the v0.1.5 fix and has not regressed.

### 6. Radio-task ownership
**MOSTLY PASS — one minor deviation noted**

All *new* Stage 11 radio work (`BECOME_HOST`, `CONNECT_TO_HOST`) is posted to the dedicated `ble_radio` task. `START_DISCOVERY` and `STOP_DISCOVERY` are also routed through the task queue.

Two pre-existing NimBLE call sites are still invoked directly from state-machine actions rather than being queued to the radio task:

- `BleService::disconnect()` — called from `on_peer_disconnected()` and `reset_and_return_start()`.
- `BleService::send_move()` — called from the local-move actions.

Both calls are quick/non-blocking and return errors (or are handled by callers), so they do not violate G12.4 or G12.6. However, they are not fully aligned with the stricter LL-042 guideline that every NimBLE call triggered by a state-machine action should be posted to the radio task or documented as a safe probe. This is a low-severity carry-over defect, not introduced by Stage 11.

### 7. In-file documentation (LL-043)
**PASS**

Every v0.1.6 bug-report-driven change has an adjacent comment citing `docs/BugReport_CYD_RPS_v0.1.5.md`:

| Change | File | Comment location |
|--------|------|------------------|
| Public-MAC manufacturer payload | `ble_service.cpp` | `start_advertising()` lines 662–665 |
| Peer public-MAC extraction | `ble_service.cpp` | `RpsAdvertisedDeviceCallbacks::onResult()` lines 64–67 |
| Public-MAC constants / fields | `ble_service.h` | `kManufacturerCompanyId` lines 60–62; `peer_public_addr_` lines 202–206 |
| Connection retry constants | `ble_service.h` | `kConnectRetries` lines 49–52; `kConnectRetryDelayMs` lines 55–58 |
| Self-discovery guard | `ble_service.cpp` | `on_peer_found()` lines 322–325 |
| Role resolution by public MAC | `ble_service.cpp` | `resolve_role()` lines 358–361 |
| HOST no-restart advertising | `ble_service.cpp` | `become_host()` lines 385–388 |
| JOIN keeps advertising | `ble_service.cpp` | `become_join()` lines 402–406 |
| Bounded connection retries | `ble_service.cpp` | `connect_to_host()` lines 435–438 |

### 8. Compile and test
**PASS**

Both requested `pio run` environments build successfully. No PlatformIO unit tests exist in the project, so `pio test` is not applicable.

---

## Defects / Action Items

| ID | Severity | Description | Recommendation |
|----|----------|-------------|----------------|
| D01 | Low | `BleService::disconnect()` and `BleService::send_move()` are called directly from state-machine actions and bypass the dedicated radio task. | Future revision: route these through the `RadioCommand` queue or document them as safe, non-blocking probes to fully satisfy LL-042. Not a Stage 11 regression and does not fail any G12 gate. |

---

## Summary

The Stage 11 v0.1.6 revision correctly fixes the asymmetric two-player BLE connection failure described in `docs/BugReport_CYD_RPS_v0.1.5.md`. The code is well-documented, compiles for both target environments, and satisfies all Stage 12 quality gates. The single low-severity deviation (direct `disconnect()` / `send_move()` calls) is pre-existing and does not block the release.

**Recommended action:** Proceed to the next workflow stage.
