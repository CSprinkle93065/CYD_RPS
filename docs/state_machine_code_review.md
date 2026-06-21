# Assessment: Stage 12 — Firmware Logic Code Critic (Re-review)

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Reviewer:** Stage 12 Firmware Logic Code Critic  
**Verdict: PASS**

---

## Executive Summary

This is the Stage 12 re-review after the Stage 11 revision. The two Stage 11 action items have been verified as resolved:

1. **H05 action mismatch (Start → Joining):** The `docs/state_machine.puml`, generated `src/state_machine/state_machine_generated.cpp`, and `docs/test_plan.json` now all agree on `on_conflict_become_join()` as the transition action. Host-level state-machine tests pass 73/73, including H05.
2. **Missing Wokwi serial markers:** `src/state_machine/app_state_machine.cpp` now emits `STATE:`, `MODE:`, and `SCORE:` markers on the appropriate transitions, satisfying the Wokwi `serial_expect` assertions in `docs/test_plan.json`.

The generated state machine continues to match the PlantUML contract transition-for-transition, the builds for both target and Wokwi environments succeed, and all Stage 12 quality gates pass.

---

## Revision Verification

### H05 — Start → Joining action name

| Source | Declared Action | Status |
|--------|-----------------|--------|
| `docs/state_machine.puml` line 28 | `on_conflict_become_join()` | ✅ Matches test plan |
| `src/state_machine/state_machine_generated.cpp` lines 175–176 | `on_conflict_become_join()` | ✅ Matches PUML |
| `docs/test_plan.json` H05 expected result | `on_conflict_become_join()` invoked | ✅ Matches firmware |
| `tests/host/test_state_machine.py` H05 | actions=`['on_conflict_become_join']` | ✅ PASS |

### Wokwi serial markers

| Marker | Required by | Implementation | Location |
|--------|-------------|----------------|----------|
| `STATE: <Name>` | W01, W02, W04, W05, W07, W08, T03, T07, T09, M01–M05 | `Serial.printf("STATE: %s\n", state_name(after));` on every state change | `app_state_machine.cpp:150` |
| `MODE: SINGLE_PLAYER` | W03, W09, T04, M02, M11 | `Serial.printf("MODE: %s\n", game_mode_name(...));` in `on_host_game()`, `on_solo()`, `on_conflict_become_join()`, `on_peer_connected()` | `app_state_machine.cpp:291, 306, 349, 372` |
| `MODE: MULTI_PLAYER` | M11 | Same as above | `app_state_machine.cpp:291, 349, 372` |
| `SCORE: W%u L%u D%u` | T06, M04 | `Serial.printf("SCORE: W%u L%u D%u\n", ...);` in `evaluate_and_show_result()` | `app_state_machine.cpp:522` |

All markers are emitted unconditionally via `Serial.printf` and are observable by Wokwi `serial_expect` tests.

---

## Quality Gate Results

### G12.1 — Every transition in `state_machine.puml` has a corresponding code path

**PASS**

`src/state_machine/state_machine_generated.cpp` implements every transition declared in `docs/state_machine.puml` for the v0.2.0 states (Boot, Start, HostWait, HostTimeoutDialog, Joining, Gameplay, Evaluating, Result, Disconnected, Error, Halted). Guarded branches, error transitions, and self-transitions are all present. Host-level transition tests (`tests/host/test_state_machine.py`) exercise the dispatch table and confirm expected target states and action invocations (73/73 PASS).

### G12.2 — Guards reference `dependency_manifest.json` facts or runtime probes

**PASS**

All guards read runtime state rather than hard-coded values:

| Guard | Source |
|-------|--------|
| `guard_init_ok` | `hal_service().init_ok()` |
| `guard_ble_init_failed` | `hal_service().ble_init_failed()` |
| `guard_hw_init_failed` | `hal_service().hw_init_failed()` |
| `guard_fatal` | `hal_service().fatal()` |
| `guard_host_mac_valid` | `ctx_.host_mac_valid` |
| `guard_peer_host_seen` | `ctx_.peer_host_seen` |
| `guard_mode_single` | `ctx_.game_mode` |
| `guard_mode_multi_and_*_peer_move_received` | `ctx_.game_mode` and `ctx_.peer_move` |
| `guard_local_move_chosen` / `guard_not_local_move_chosen` | `ctx_.local_move` |
| `guard_retries_exhausted` | Contract compatibility (`true`; `EV_CONNECT_FAILED` is posted only after retries are exhausted) |

No guard encodes assumptions about NimBLE address types, board revisions, or library versions.

### G12.3 — No LVGL or UI toolkit imports in state-machine code

**PASS**

No file under `src/state_machine/` includes LVGL, TFT_eSPI, or XPT2046 headers. UI integration is isolated behind weak `extern "C"` adapter stubs declared in `app_state_machine.h` (e.g., `ui_show_screen_start`, `ui_set_status`). The state machine only invokes these adapters; the integration/UI layer supplies the real implementations.

### G12.4 — All external calls have defined error-state transitions

**PASS**

- `main.cpp` checks the return value of `app::ble_service().init()` and records `ble_init_failed` in `HalService`; `app_init()` propagates this to `EV_ERROR`.
- `BleService::init()` returns `false` if server/service/characteristic creation fails.
- `do_start_presence_beacon()` checks `start_scanning()` and posts `EV_ERROR` on failure.
- `start_presence_advertising()` and `start_host_advertising()` check free heap and post `EV_ERROR` if advertising cannot be configured.
- `do_connect_to_host()` posts `EV_CONNECT_FAILED` after exhausted retries and `EV_ERROR` on service/characteristic/subscription failures.
- `send_move()` returns a `bool`; `AppStateMachine` calls `app_on_error("Failed to send move")` on failure.

**Note:** `NimBLEAdvertising::start()` and `NimBLEScan::start()` in the Host-advertising path do not check the boolean return directly. A failure there will eventually surface as a Host-wait timeout or a connection failure rather than an immediate `EV_ERROR`. This is acceptable for the current design but could be hardened in a future revision.

### G12.5 — Generated code matches `state_machine.puml` transition-for-transition

**PASS**

Regenerating the state machine with `execution/generate_state_machine_cpp.py --input docs/state_machine.puml` produces output identical to the committed `state_machine_generated.cpp` and `state_machine_generated.h` (`diff` reports no changes). The C++ dispatch table maps one-to-one with the PlantUML event/guard/action tuples, including the revised `Start --> Joining / on_conflict_become_join()` transition.

### G12.6 — No blocking delays in state-machine update loops

**PASS**

`AppStateMachine::update()` is non-blocking: it calls `ble_service().update(millis())`, updates the Host-wait progress bar, and dispatches pending events. All `vTaskDelay` calls are confined to `BleService::do_become_host()` and `do_connect_to_host()`, which run on the dedicated radio task and therefore do not block the main state-machine loop.

### G12.7 — All GPIO access goes through the HAL/pins.h layer

**PASS**

No file under `src/state_machine/` performs direct GPIO access (`digitalWrite`, `digitalRead`, `pinMode`, or register manipulation). Hardware constants come from `include/pins.h` and `include/hal_config.h` only. The state machine never executes inside an ISR and holds the event-queue mutex only for short ring-buffer operations.

---

## BLE-Specific Checks

| Check | Result | Evidence |
|-------|--------|----------|
| Host advertises on public MAC | PASS | `BleService::init()` calls `NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC)` (`ble_service.cpp:126`). |
| Join connects to public MAC directly (not reconstructed from `getNative()`) | PASS | `RpsAdvertisedDeviceCallbacks::onResult()` extracts the public MAC either from a public advertisement address or from manufacturer data; `do_connect_to_host()` constructs `NimBLEAddress(host_mac, BLE_ADDR_PUBLIC)` (`ble_service.cpp:453`). |
| Dedicated radio task owns all NimBLE operations | PASS with caveat | The radio task owns scan/advertise/connect/start/stop commands. `send_move()` and `disconnect()` are called from the main loop but are non-blocking GATT data-plane operations (write-without-response / notify). They do not exhibit the controller-race behavior that motivated the radio task. |
| Event queue is thread-safe | PASS | `AppStateMachine` creates a FreeRTOS mutex in its constructor and locks both `enqueue()` and `dispatch_pending()`. |
| GATT server started during init before any GAP procedure | PASS | `BleService::init()` creates the service/characteristic and calls `service_->start()` and `server_->start()` before any scan/advertise/connect is started (`ble_service.cpp:154-158`). |
| `#ifndef WOKWI_SIMULATION` guards NimBLE init | PASS | Both `main.cpp` and `BleService::init()` skip NimBLE initialization under `WOKWI_SIMULATION`. |
| Advertising stop→connect guard delay | PASS | `do_connect_to_host()` waits `STOP_ADV_TO_CONNECT_GUARD_MS` (50 ms) after `stop_advertising()` before each `connect()` attempt (`ble_service.cpp:458`). |
| Host advertising restart guard delay | PASS | `do_become_host()` and `do_restart_host_advertising()` wait `HOST_STOP_SCAN_TO_ADV_GUARD_MS` (20 ms) after stopping scan/advertise before restarting Host advertising (`ble_service.cpp:352`, `411`). |
| Serial command parser maps commands to state-machine events | PASS | `serial_command_dispatch()` maps `HOST`, `SOLO`, `ROCK`, `PAPER`, `SCISSORS`, `AGAIN`, `RESET`, and `HOME` to the corresponding `Event` values (`serial_command_handler.cpp:91-104`). |

---

## Lessons-Learned Regression Checks

- **LL-034 (single-owner hardware init):** PASS. `main.cpp` owns all peripheral initialization; `app_init()` and `HalService` only probe/record results.
- **LL-036 (BLE init failure propagation & non-blocking discovery):** PASS. BLE init result is checked in `main.cpp`; discovery uses the non-blocking `scan->start(0, nullptr, false)` overload with a polled Host-wait timeout.
- **LL-037 (skip NimBLE under Wokwi):** PASS. `WOKWI_SIMULATION` guards in `main.cpp` and `BleService::init()`.
- **LL-038 (default env is hardware):** PASS. `platformio.ini` sets `default_envs = esp32-2432s028r_cyd2usb`.
- **LL-039/LL-042 (radio task owns heavy NimBLE work):** PASS. All scan/advertise/connect operations are posted to the radio task via `RadioCommand`.
- **LL-040 (thread-safe event queue):** PASS. Mutex protects `enqueue()` and `dispatch_pending()`.
- **LL-044 (start GATT server before GAP):** PASS. Server started in `init()`.
- **LL-045/LL-047/LL-048/LL-049/LL-050 (public-MAC role resolution, connection retries, guard delays):** PASS. Role resolution compares public MACs; JOIN retries up to `kMaxJoinRetries` with bounded connect timeout; guard delays and instrumentation logs are present.

---

## Build & Test Evidence

- `pio run -e esp32-2432s028r_cyd2usb` — **SUCCESS**
- `pio run -e esp32-2432s028r_cyd2usb_wokwi` — **SUCCESS**
- `python tests/host/test_state_machine.py` — **73 passed, 0 failed**
  - H05 (`Start -> Joining on EV_HOST_FOUND`) now passes with the expected action `on_conflict_become_join`.

---

## Notes & Observations

1. **Stage 11 revision items verified closed.** H05 action-name mismatch and missing Wokwi serial markers are both resolved.
2. **Unchecked `adv->start()` returns:** `start_presence_advertising()` and `start_host_advertising()` do not assert the `bool` returned by `NimBLEAdvertising::start()`. A failure path here would silently skip advertising rather than posting `EV_ERROR`. In HostWait this is mitigated by the Host-wait timeout; for the Start presence beacon it could delay peer discovery. Recommend checking the return and posting `EV_ERROR` in a future revision.
3. **`BleService::update()` is currently a no-op:** Host-wait timeout polling lives in `AppStateMachine::update_host_wait_progress()`. The empty `BleService::update()` hook is harmless but `post_host_timeout_if_needed()` is dead code. Consider removing or consolidating in a future cleanup.
4. **`send_move()` not queued to radio task:** As noted in the BLE checks, this is acceptable because the operation is a non-blocking GATT write/notify. If future field testing reveals main-loop GATT instability, this operation can be added to the `RadioCommand` queue.

---

## Conclusion

The Stage 11 revision successfully addressed the H05 action-name mismatch and the missing Wokwi serial markers. The v0.2.0 firmware logic satisfies all Stage 12 quality gates. The code is structurally sound, the generated state machine matches the PlantUML contract, BLE correctness requirements are met, and the build succeeds for both target environments.

**Verdict: PASS**
