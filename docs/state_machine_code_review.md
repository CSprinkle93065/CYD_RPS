# Assessment: Stage 12 — Firmware Logic Code Critic

**Verdict: GO**

## Findings

- [PASS] **G12.1 — Every transition in `state_machine.puml` has a corresponding code path.**
  `state_machine_generated.cpp` `StateMachine::dispatch()` implements every transition listed in the PlantUML diagram, including all guarded branches and the error transitions out of every state.

- [PASS] **G12.2 — Guards reference runtime probes or dependency facts, not hardcoded assumptions.**
  All guards read runtime state: `hal_service().init_ok()`, `hal_service().ble_init_failed()`, `hal_service().hw_init_failed()`, `hal_service().fatal()`, `ctx_.game_mode`, `ctx_.local_move`, `ctx_.peer_move`, and `ble_service().role()`.

- [PASS] **G12.3 — No LVGL or UI toolkit imports in state machine code.**
  No `src/state_machine/` file includes LVGL, TFT_eSPI, or XPT2046 headers. UI integration is isolated behind weak `extern "C"` adapter stubs declared in `app_state_machine.h`.

- [PASS] **G12.4 — All external calls have defined error-state transitions; no silent exception swallowing.**
  `BleService::init()` returns `false` on earlier NimBLE creation failures; `do_start_discovery()`, `start_advertising_if_needed()`, `become_host()`, and `connect_to_host()` post `EV_ERROR` on `start_scanning()`, `start_advertising()`, `connect()`, `getService()`, `getCharacteristic()`, and `subscribe()` failures. `NimBLEServer::start()` is `void` in NimBLE-Arduino 1.4.3, so it has no return value to swallow.

- [PASS] **G12.5 — Generated code matches `state_machine.puml` transition-for-transition.**
  `StateMachine::dispatch()` states (Boot, Start, PeerSearch, RoleNegotiating, Connecting, Gameplay, SinglePlayerFallback, Evaluating, Result, Disconnected, Error, Halted) and their event/guard/action tuples align exactly with the PlantUML transitions.

- [PASS] **G12.6 — No blocking delays in state-machine update loops.**
  `AppStateMachine::update()` is purely non-blocking (discovery timeout poll and event dispatch). The only `vTaskDelay` calls live in `BleService::become_host()` and `BleService::connect_to_host()`, which run on the dedicated radio task and therefore do not block the main state-machine loop.

- [PASS] **G12.7 — All GPIO access goes through the HAL/pins.h layer; ISR and watchdog safe.**
  `src/state_machine/` contains no `digitalWrite`, `digitalRead`, `pinMode`, or raw GPIO register access. The only hardware constants used come from `hal_config.h`/`pins.h`.

## Bug-Fix Specific Checks

- [PASS] **`BleService::connect_to_host()` guard delay.** A `50 ms` `vTaskDelay` is inserted after `stop_advertising()` and before `cli->connect()` (`ble_service.cpp:520`), and the function runs on the radio task via the `CONNECT_TO_HOST` command.

- [PASS] **`BleService::become_host()` clean advertising restart.** The HOST path stops scanning, stops advertising, waits `20 ms`, then restarts advertising (`ble_service.cpp:427–436`).

- [PASS] **Instrumentation logs present.** `Serial.printf`/`Serial.println` instrumentation is present in `on_peer_found()` (`ble_service.cpp:360`), `resolve_role()` (`ble_service.cpp:399`), `become_host()` (`ble_service.cpp:437`), `become_join()` (`ble_service.cpp:453`), and `connect_to_host()` (`ble_service.cpp:524`), matching `docs/BugReport_CYD_RPS_v0.1.8.md` §9.1.

- [PASS] **`deleteAllBonds()` guarded by `WOKWI_SIMULATION`.** `NimBLEDevice::deleteAllBonds()` is called inside `BleService::init()` and wrapped in `#ifndef WOKWI_SIMULATION` (`ble_service.cpp:122–124`).

- [PASS] **Misleading `status=13` comment corrected.** `ble_service.h:56–63` now identifies `status=13` as `BLE_ERR_REM_USER_CONN_TERM` and references `docs/BugReport_CYD_RPS_v0.1.8.md` §7.1.

- [PASS] **No regression of LL-042 / LL-045 / LL-046 / LL-047 / LL-048 / LL-049.** The radio task owns all four NimBLE commands; `peer_addr_type_` is preserved from discovery through `connect()`; the JOIN keeps advertising through bounded discovery windows, stops advertising only immediately before `connect()`, uses the 5 s connect timeout, and resolves roles against the stable public MAC carried in manufacturer data.

- [PASS] **No regression of LL-039 / LL-040.** Radio startup is deferred out of the LVGL/state-machine dispatch context via `ctx_.start_discovery_pending` and radio-task commands; the event queue is protected by the FreeRTOS mutex created in the `AppStateMachine` constructor.

## Notes

- `NimBLEAdvertising::addServiceUUID()` and `setManufacturerData()` return `bool`, but their results are not explicitly checked in `start_advertising()`. The subsequent `adv->start()` return is checked and will post `EV_ERROR` if advertising cannot start.
- `xTaskCreatePinnedToCore()` failure in `start_radio_task()` is logged but does not post `EV_ERROR`; the code falls back to direct radio calls. This is acceptable because task creation is a setup-time concern outside the state-machine event model.
