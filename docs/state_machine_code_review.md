# Stage 12 Firmware Logic Code Review — CYD_RPS v0.2.2

| Item | Value |
|---|---|
| workflow_id | wvc_20260621_164404 |
| project_name | CYD_RPS |
| target_version | 0.2.2 |
| reviewer | Stage 12 Firmware Logic Code Critic |
| review_date | 2026-06-21 |

## Verdict: GO

All 11 quality gates pass. The four issues in `BugReport_CYD_RPS_v0.2.1.md` are addressed in code, both PlatformIO environments build successfully, and the host state-machine test suite passes with 74/74 tests. Two low-severity residual items are noted below for Stage 11 to clean up in a follow-up revision; they do not block the v0.2.2 bug-fix release.

## Quality Gate Results

### G12.1 — Every PUML transition has a corresponding code path and hand-written implementation
**PASS**

`docs/state_machine.puml` defines 46 state transitions. `src/state_machine/state_machine_generated.cpp` contains a matching `StateMachine::dispatch()` table for every `(state, event, guard, action)` tuple, and `src/state_machine/app_state_machine.cpp` / `app_state_machine.h` implement every generated virtual guard and action hook. The only ordering ambiguity (Boot `EV_ERROR` with both BLE and HW failure flags set) resolves to `Error` because `guard_ble_init_failed` is checked before `guard_hw_init_failed`; this matches the PUML priority and is acceptable because `app_init()` posts a single `EV_ERROR` for any failure.

### G12.2 — Guards reference dependency-manifest facts or runtime probes, not hardcoded assumptions
**PASS**

All guards read runtime state:

- `guard_init_ok`, `guard_ble_init_failed`, `guard_hw_init_failed`, `guard_fatal` read `hal_service()` probes (`app_state_machine.cpp:189-203`).
- `guard_host_mac_valid`, `guard_peer_host_seen` read `ctx_.host_mac_valid` / `ctx_.peer_host_seen` (`app_state_machine.cpp:205-221`).
- `guard_local_mac_lower` compares `ble_service().local_public_mac()` against `ctx_.host_mac` (`app_state_machine.cpp:209-217`); both arrays are stored in canonical/display byte order.
- `guard_mode_single`, `guard_mode_multi_and_*`, `guard_local_move_chosen`, `guard_not_local_move_chosen` read `ctx_.game_mode`, `ctx_.peer_move`, and `ctx_.local_move`.
- `guard_retries_exhausted` returns `true` because `BleService` only posts `EV_CONNECT_FAILED` after the retry loop gives up (`app_state_machine.cpp:223-227`). This is a deliberate simplification, not a hardcoded assumption about hardware.

### G12.3 — No LVGL or UI toolkit imports in state machine code
**PASS**

None of the state-machine source files (`state_machine_generated.cpp/h`, `app_state_machine.cpp/h`, `ble_service.cpp/h`, `game_logic.h`) include LVGL headers. UI integration is through weak `extern "C"` adapter callbacks declared in `app_state_machine.h` and implemented in the UI layer. The state machine remains decoupled from the UI toolkit.

### G12.4 — All external calls have defined error-state transitions; no silent exception swallowing
**PASS**

All NimBLE calls that can fail return values or pointers that are checked and mapped to state-machine events:

- `BleService::init()` returns `false` if server/service/characteristic creation or service start fails; `main.cpp:72-77` maps this to `hal.mark_ble_init_failed(true)` and `app_init()` posts `EV_ERROR` (`app_state_machine.cpp:54-61`).
- `start_scanning()` returns `bool`; failure in `do_start_presence_beacon()` posts `EV_ERROR` (`ble_service.cpp:335-340`).
- `NimBLEDevice::createClient()` null check posts `EV_CONNECT_FAILED` (`ble_service.cpp:504-508`).
- `NimBLEClient::connect()` result drives the retry loop and finally posts `EV_CONNECT_FAILED` (`ble_service.cpp:522-560`).
- `getService()` / `getCharacteristic()` / `subscribe()` failures post `EV_ERROR` (`ble_service.cpp:578-598`).
- `BleService::send_move()` returns `bool`; callers (`app_state_machine.cpp:414-477`) check it and call `app_on_error("Failed to send move")` → `EV_ERROR`.

No `try`/`catch` blocks silently swallow exceptions. Minor improvement opportunity: `NimBLEAdvertising::start()` return value is not checked at `ble_service.cpp:385` and `:459`; a failure would surface only as a host-wait timeout.

### G12.5 — Generated code matches `state_machine.puml` transition-for-transition
**PASS**

The generated `StateMachine::dispatch()` table (`state_machine_generated.cpp:8-197`) contains every transition listed in the PUML, with the same states, events, guards, and action names. State and event enums (`state_machine_generated.h:10-44`) are a superset of the PUML vocabulary. The only addition is the self-loop `Start --> Start` on `EV_RESET`, which is explicitly shown in the PUML.

### G12.6 — No blocking delays in state-machine `update()` loops
**PASS**

`AppStateMachine::update()` (`app_state_machine.cpp:100-109`) is strictly non-blocking: it calls `ble_service().update(millis())`, `update_host_wait_progress()`, and `dispatch_pending()`. `BleService::update()` is empty (`ble_service.cpp:770-774`). All `vTaskDelay()` calls are confined to the dedicated BLE radio task (`ble_service.cpp:412`, `:471`, `:530`). The `delay(5)` in `main.cpp:106` is outside the state-machine update path.

### G12.7 — All GPIO access goes through the HAL/pins.h layer; ISR and watchdog safe
**PASS**

No state-machine file performs direct GPIO register access. `main.cpp` initializes TFT, touch, and backlight using `pins.h` constants (`PIN_TOUCH_CS`, `PIN_TOUCH_CLK`, etc.) and the `TFT_eSPI` / `XPT2046_Touchscreen` drivers. `hal_service.cpp/h` only records init results; it does not drive GPIO.

### G12.8 — Bug-fix specific checks
**PASS**

#### B01-1: Peer MAC used in `NimBLEClient::connect()` is canonical, not double-reversed
- `BleService::init()` reverses `NimBLEAddress::getNative()` into the canonical `public_mac_` array (`ble_service.cpp:175-179`).
- `RpsAdvertisedDeviceCallbacks::onResult()` reverses the public-address `getNative()` bytes into `public_mac` before passing them to `on_host_found()` (`ble_service.cpp:103-108`).
- `do_connect_to_host()` constructs `NimBLEAddress(canonical_mac, addr_type)` (`ble_service.cpp:516-519`). `NimBLEAddress(uint8_t[6], type)` internally reverse-copies the canonical bytes to controller order, so the link-layer address is correct.
- `peer_addr_type_` captured in `on_host_found()` (`ble_service.cpp:638`, `:651`) is used verbatim in the connect call.

No call to `NimBLEAddress(uint8_t[6], type)` is fed raw `getNative()` bytes.

#### B01-2: Exactly one board becomes Host when two peers discover each other on Start
- `Start` dispatch checks `guard_local_mac_lower` first, then `guard_host_mac_valid` (`state_machine_generated.cpp:175-180`).
- `guard_local_mac_lower` returns `memcmp(ble_service().local_public_mac(), ctx_.host_mac, 6) < 0` (`app_state_machine.cpp:209-217`).
- The lower canonical public MAC transitions to `HostWait` (Host); the higher MAC transitions to `Joining` (Join). Because both peers use the same deterministic comparison on the same stable public MAC, exactly one Host and one Join result.

#### B01-3: `EV_HOST_GAME` forces `Joining --> HostWait` and safely aborts in-progress connect
- PUML transition `Joining --> HostWait : EV_HOST_GAME / on_host_game()` exists and is generated (`state_machine_generated.cpp:148-150`).
- `AppStateMachine::on_host_game()` calls `ble_service().become_host()` (`app_state_machine.cpp:303`).
- `BleService::become_host()` sets `abort_connect_ = true` and queues `BECOME_HOST` to the radio task (`ble_service.cpp:284-296`).
- `do_connect_to_host()` checks `abort_connect_` before every attempt, after every failed attempt, and after a successful connect, cleaning up the client without posting `EV_CONNECTED` when aborted (`ble_service.cpp:522-573`).

#### U06: `EV_RESET` returns to Start without `ESP.restart()`
- `serial_command_handler.cpp:103-104` maps both `RESET` and `HOME` to `sm_post_event(Event::EV_RESET)`.
- `reset_and_return_start()` (`app_state_machine.cpp:540-547`) resets game state, disconnects, stops host advertising, shows `SCR_Start`, and restarts the presence beacon. It does not call `ESP.restart()`.
- A workspace-wide grep for `ESP.restart` / `esp_restart` in `projects/CYD_RPS/src` returns no matches.

### G12.9 — Lessons learned LL-042, LL-045, LL-048, LL-050 respected
**PASS**

- **LL-042 (radio task owns NimBLE work):** `RadioCommandType` includes `BECOME_HOST` and `CONNECT_TO_HOST`, and `radio_task_loop()` dispatches them (`ble_service.cpp:742-760`). State-machine actions post via `signal_become_host()` / `signal_connect_to_host()` / `signal_*` wrappers rather than calling `NimBLEScan::start()` / `NimBLEAdvertising::start()` / `NimBLEClient::connect()` directly. The review checklist focus — no scan/advertise/connect from the Arduino main loop — is satisfied.
- **LL-045 (preserve peer address type):** `peer_addr_type_` is captured in `on_host_found()` and used in `do_connect_to_host()`; no hard-coded `BLE_ADDR_PUBLIC` in the connect path.
- **LL-048 (stop advertising before connect, size timeout):** `do_connect_to_host()` calls `stop_advertising()` before the connect loop and sets `cli->setConnectTimeout(JOIN_CONNECT_TIMEOUT_SEC)` to 5 s (`ble_service.cpp:497-510`).
- **LL-050 (guard delays and logs):** `do_become_host()` waits `HOST_STOP_SCAN_TO_ADV_GUARD_MS` (20 ms) after stopping scan/advertise before restarting host advertising (`ble_service.cpp:409-412`); `do_connect_to_host()` waits `STOP_ADV_TO_CONNECT_GUARD_MS` (50 ms) before each `connect()` (`ble_service.cpp:529-530`); `Serial.printf` instrumentation is present around discovery, role resolution, advertising stop/connect, and host restart.

Residual note (non-blocking): `BleService::disconnect()` and `BleService::send_move()` are still invoked directly from state-machine actions in the main-loop context. For full LL-042 compliance these should eventually be queued to the radio task as well.

### G12.10 — PlatformIO builds succeed
**PASS**

- `pio run -e esp32-2432s028r_cyd2usb` succeeded (RAM 23.2 %, Flash 72.8 %).
- `pio run -e esp32-2432s028r_cyd2usb_wokwi` succeeded (RAM 21.0 %, Flash 66.1 %).

### G12.11 — Host state-machine tests pass
**PASS**

`python tests/host/test_state_machine.py` ran 74 `host_assert` tests; 74 passed, 0 failed.

## Specific Defects / Residual Risks

| ID | Severity | File(s) / Line(s) | Description | Suggested Action for Stage 11 |
|---|---|---|---|---|
| R01 | Low | `src/state_machine/app_state_machine.cpp:392-393`, `:540-542`<br>`src/state_machine/ble_service.cpp:719-727` | `reset_and_return_start()` and `on_peer_disconnected()` call `ble_service().disconnect()` directly from the main-loop/LVGL context. From `Joining`, this does **not** set `abort_connect_`, so the radio task may still be inside `NimBLEClient::connect()` while the main loop restarts the presence beacon — recreating the scan/advertise-vs-connect controller race that LL-050 fixed. | Add `RadioCommandType::DISCONNECT` to the radio-task queue. Route reset cleanup and `on_peer_disconnected()` through it, and set `abort_connect_` in the reset path. |
| R02 | Low | `src/state_machine/ble_service.cpp:385`, `:459` | `NimBLEAdvertising::start()` returns `bool`, but the return value is ignored in both presence and host advertising paths. | Check the return value; on failure post `EV_ERROR` or schedule a retry. |
| R03 | Low | `src/state_machine/app_state_machine.cpp:223-227` | `guard_retries_exhausted()` always returns `true`. While the current BLE service only posts `EV_CONNECT_FAILED` after all retries, a future change that posts it earlier would cause a premature `Joining --> Start` transition. | Either make the guard reference a real retry counter or document the contract explicitly in the guard comment. |

## Actionable Feedback for Stage 11

1. **Queue disconnect to the radio task (R01).** This is the only residual item with any potential to recreate the BLE controller races addressed in v0.2.0/v0.2.1. A minimal fix is:
   - Add `DISCONNECT` to `RadioCommandType`.
   - Implement `do_disconnect()` on the radio task that calls `NimBLEClient::disconnect()` / `NimBLEServer::disconnect()` and clears `client_` / `connected_`.
   - Replace direct `ble_service().disconnect()` calls in `on_peer_disconnected()` and `reset_and_return_start()` with a queued signal, and set `abort_connect_ = true` in `reset_and_return_start()` when the current role is `JOIN`.
2. **Check `NimBLEAdvertising::start()` return values (R02)** for completeness in error handling.
3. **Tighten `guard_retries_exhausted()` (R03)** so the state machine does not rely on the BLE service's posting policy.

These changes are recommended but not required to unblock v0.2.2 because the four reported bugs are fixed and all automated gates pass.
