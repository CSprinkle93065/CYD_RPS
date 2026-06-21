# Firmware Logic Code Review: CYD_RPS v0.2.1 — Stage 12

**Workflow ID:** wvc_20260618_011606  
**Project:** CYD_RPS  
**Target Version:** 0.2.1  
**Reviewer:** Firmware Logic Code Critic (Stage 12)  
**Inputs Reviewed:**
- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.2.0.md` (issue B01)
- `projects/CYD_RPS/docs/state_machine.puml`
- `projects/CYD_RPS/docs/state_machine_contract.json`
- `projects/CYD_RPS/dependency_manifest.json`
- `projects/CYD_RPS/src/state_machine/state_machine_generated.cpp`
- `projects/CYD_RPS/src/state_machine/ble_service.cpp`
- `projects/CYD_RPS/src/state_machine/ble_service.h`
- `projects/CYD_RPS/src/state_machine/app_state_machine.cpp`
- `known_issues.md`
- `lessons_learned.md` (LL-036, LL-039, LL-040, LL-042, LL-043, LL-044, LL-045, LL-046, LL-047, LL-048, LL-049, LL-050)

**Verdict:** GO

---

## Quality Gate Results

| Gate | Result | Evidence |
|------|--------|----------|
| G12.1 | PASS | Every transition declared in `state_machine.puml` is represented in `state_machine_generated.cpp` `StateMachine::dispatch()`. Boot (3), Start (5), HostWait (6), HostTimeoutDialog (4), Joining (4), Gameplay (14), Evaluating (3), Result (4), Disconnected (3), Error (3), and Halted (0 terminal) all match transition-for-transition. |
| G12.2 | PASS | All guards read runtime state or HAL probes rather than hard-coded assumptions: `guard_init_ok`/`guard_ble_init_failed`/`guard_hw_init_failed`/`guard_fatal` query `hal_service()`; mode/move guards query `GameContext` fields populated by actions and BLE callbacks. `dependency_manifest.json` facts (board, NimBLE version, Wokwi flag) are consumed at compile/init time, not in guards. |
| G12.3 | PASS | `state_machine_generated.cpp`, `app_state_machine.cpp`, `ble_service.cpp`, and `ble_service.h` contain no `#include <lvgl...>` or LVGL object usage. The state machine only calls weak UI adapter functions; the UI layer is responsible for LVGL. |
| G12.4 | PASS | External/BLE call errors are mapped to state-machine events: `BleService::init()` return is checked in `main.cpp` and drives `EV_ERROR` via `app_init()`; `do_connect_to_host()` posts `EV_CONNECT_FAILED` or `EV_ERROR`; `getService`/`getCharacteristic`/`subscribe` failures post `EV_ERROR`; `send_move()` failures in actions call `app_on_error("Failed to send move")` which posts `EV_ERROR`. No exceptions are silently swallowed. |
| G12.5 | PASS | `state_machine_generated.cpp` was produced by `generate_state_machine_cpp.py` from `state_machine.puml`. State names, event names, guard names, action names, and target states match the PUML source. The transition matrix in `dispatch()` is a literal encoding of the PUML diagram. |
| G12.6 | PASS | `AppStateMachine::update()` (`app_state_machine.cpp:100-109`) is non-blocking: it calls `ble_service().update()`, updates host-wait progress, and dispatches pending events with no `delay()` or busy-wait. The intentional guard delays (`vTaskDelay(20 ms)` in `do_become_host()`/`do_restart_host_advertising()` and `vTaskDelay(50 ms)` in `do_connect_to_host()`) run on the dedicated BLE radio task, not in the state-machine update loop. |
| G12.7 | PASS | State-machine files contain no direct GPIO register access. `main.cpp` owns GPIO initialization and uses constants from `include/pins.h` and `include/hal_config.h`. `hal_service.h`/`hal_service.cpp` only record init-probe results; they do not drive pins. |

---

## Additional Checks

### B01 — Bluetooth auto-join filtering

**[PASS]** `RpsAdvertisedDeviceCallbacks::onResult()` in `ble_service.cpp:52-120` now accepts a Host advertisement when **either** the RPS service UUID is present (`advertisedDevice->isAdvertisingService(service_uuid)`, line 62) **or** the manufacturer data matches the presence-beacon signature (`PRESENCE_COMPANY_ID == 0x00FF`, 4-character device ID, 6-byte public MAC, lines 67-89).

**[PASS]** The 4-character device ID in manufacturer data is validated as hexadecimal (`'0'-'9'`, `'A'-'F'`, `'a'-'f'`) at lines 75-83 before the beacon is accepted.

**[PASS]** Public MAC extraction follows the B01 rule: if the advertisement address type is `BLE_ADDR_PUBLIC`, the MAC is copied from `addr.getNative()` (lines 102-104); otherwise it is taken from the manufacturer-data payload (lines 107-109).

**[PASS]** Discovered peer address type is preserved: `onResult()` passes `addr.getType()` to `on_host_found()` (line 118), which stores it in `peer_addr_type_` (`ble_service.h:192`). `do_connect_to_host()` constructs `NimBLEAddress(host_mac, peer_addr_type_)` (lines 491-493). The only `BLE_ADDR_PUBLIC` fallback is defensive (when `peer_addr_type_ == 0xFF`), matching the "captured and used verbatim" requirement for the normal flow.

### LL-036 — Propagate BLE init failures / non-blocking discovery

**[PASS]** `main.cpp:72-78` checks `ble_service().init()` and records `ble_init_failed` before `app_init()` posts `EV_ERROR` or `EV_BOOT_DONE`. `BleService::start_scanning()` uses the non-blocking callback overload `scan->start(0, nullptr, false)` (`ble_service.cpp:792`).

### LL-039 / LL-042 — Radio-task ownership of NimBLE operations

**[PASS]** All scan/advertise/connection operations initiated by state-machine actions are posted to the dedicated radio task:
- `signal_start_presence_beacon()` / `signal_stop_presence_beacon()`
- `become_host()` (posts `BECOME_HOST` when the radio task is ready)
- `signal_stop_host_advertising()` / `signal_restart_host_advertising()`
- `signal_connect_to_host()`

**[NOTE]** Two NimBLE operations remain direct calls from actions rather than radio-task commands:
- `BleService::send_move()` is called directly from `on_local_move_*_complete()` / `on_local_move_*_pending()` (`app_state_machine.cpp:412, 423, 434, 445, 456, 467`). It performs a quick GATT notify/write and returns immediately; failures are surfaced via `app_on_error()`.
- `BleService::disconnect()` is called directly from `on_peer_disconnected()` and `reset_and_return_start()` (`app_state_machine.cpp:388, 537`). It terminates the local link and returns immediately.

Neither call blocks the state-machine update loop, and neither silently swallows errors. They are outside the scope of the original LL-042 failure pattern (scan/advertise/connect stalling or resetting the device from the LVGL stack), so they do not fail G12.4 or G12.6. Future hardening could route them through the radio task for symmetry, but this is not required for v0.2.1.

### LL-040 — Thread-safe event queue

**[PASS]** `AppStateMachine` creates a FreeRTOS mutex in its constructor (`app_state_machine.cpp:80`), takes it in `enqueue()` and `dispatch_pending()`, and gives it back afterward (`app_state_machine.cpp:155-165`). BLE callbacks post events via `sm_post_event()` from a different task, so the mutex protects the ring buffer.

### LL-043 — Bug-report rationale comments

**[PASS]** B01 and LL-045 are referenced in adjacent comments wherever the filtering logic or address-type handling changed:
- `ble_service.cpp:55-61` — B01 rationale for accepting manufacturer-data beacon.
- `ble_service.cpp:95-97` — B01 rationale for MAC extraction fallback.
- `ble_service.cpp:116-117` — B01 / LL-045 address-type preservation.
- `ble_service.h:9, 14-16, 190-192` — header-level B01 / LL-045 references.

### LL-044 — GATT server started before GAP procedures

**[PASS]** `BleService::init()` creates the NimBLE server, service, and characteristic and calls `service_ptr(service_)->start()` and `srv->start()` (lines 191-195) before any scan/advertise/connect procedure can begin.

### LL-045 — Preserve BLE peer address type

**[PASS]** Address type is captured in `onResult()`, stored in `peer_addr_type_`, reset at the start of each discovery/advertising cycle (`do_start_presence_beacon()` line 311, `do_become_host()` line 381), and used in `do_connect_to_host()` line 492. No hard-coded `BLE_ADDR_PUBLIC` is used in the normal discovery→connection path.

### LL-046 / LL-047 / LL-048 / LL-049 / LL-050 — Role negotiation, advertising stop/connect guard delays, and retries

**[PASS]** The v0.2.0 negotiation behavior is preserved and improved:
- Public MAC is embedded in manufacturer data (`start_host_advertising()` lines 427-434 and `start_presence_advertising()` lines 352-359) for symmetric role resolution (`on_host_found()` lines 569-587).
- JOIN keeps advertising until role is resolved, then stops advertising only immediately before each `connect()` attempt (`do_connect_to_host()` lines 473-474, 498).
- Bounded retry loop uses `GameContext::kMaxJoinRetries` and a 5-second connect timeout (`do_connect_to_host()` lines 496-509).
- Guard delays are present: 20 ms after stopping scan before host advertising (`do_become_host()` line 391), 20 ms in `do_restart_host_advertising()` line 450, and 50 ms after stopping advertising before each `connect()` (`do_connect_to_host()` line 498).

**[NOTE]** Instrumentation logs are present for peer discovery (`DISCOVERY: host seen via...`, lines 105-112; `DISCOVERY: peer ...`, lines 564-567), role resolution (`ROLE: local MAC lower/higher/auto-join`, lines 574, 578, 590), and JOIN connect attempts (`JOIN: connecting...`, `JOIN: connect attempt failed`, lines 500-512). An explicit log around the HOST advertising restart in `do_become_host()` / `do_restart_host_advertising()` is not present; adding one would aid future two-board diagnosis but is not a gate failure.

### LL-034 — Single-owner hardware initialization

**[PASS]** `hal_service.h:5-8` explicitly states that `HalService` does not initialize hardware; it only records probe results. `main.cpp:42-78` owns the single initialization sequence for TFT, touch, BLE, and UI. State-machine actions do not call `init()`/`begin()` on peripherals.

---

## Summary

All seven quality gates pass. The Stage 11 firmware logic changes correctly implement the B01 fix (accept service UUID or manufacturer-data beacon, validate hex device ID, preserve address type), maintain radio-task ownership of scan/advertise/connect operations, keep the state-machine update loop non-blocking, and propagate BLE errors into PUML-defined transitions. The code is ready to proceed.
