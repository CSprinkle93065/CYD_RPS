# Assessment: Stage 14 — Integration Code Critic

**Verdict:** GO

## Findings

- [PASS] **G14.1 — Integration Agent modified NO files owned by Display & Input or Firmware Logic agents.**
  Working-tree changes for this revision are limited to Firmware Logic and its Stage 12 review artifact:
  - `src/state_machine/ble_service.cpp`
  - `src/state_machine/ble_service.h`
  - `docs/state_machine_code_review.md`
  - `docs/BugReport_CYD_RPS_v0.1.8.md` (untracked)

  Integration-owned files (`src/integration/integration.cpp`, `src/integration/integration.h`, `src/main.cpp`) and Display & Input owned files (`src/ui/*`) are unchanged relative to `HEAD`; `git diff HEAD -- src/integration src/main.cpp src/ui` produces no output. Therefore the Integration Agent did not modify files owned by the other agents.

- [PASS] **G14.2 — All contract bindings are implemented.**
  `docs/state_machine_contract.json` declares seven clickable controls and four state-driven UI update families. All are implemented and wired:

  | Control / Adapter | Contract Action | Implementation |
  |-------------------|-----------------|----------------|
  | `btnPlay` | `on_play_button_clicked() -> post_event(EV_PLAY)` | `src/ui/ui.cpp:48-53` |
  | `btnCancel` | `on_cancel_button_clicked() -> post_event(EV_CANCEL)` | `src/ui/ui.cpp:55-60` |
  | `btnRock` | `on_rock_button_clicked() -> post_event(EV_MOVE_ROCK)` | `src/ui/ui.cpp:62-67` |
  | `btnPaper` | `on_paper_button_clicked() -> post_event(EV_MOVE_PAPER)` | `src/ui/ui.cpp:69-74` |
  | `btnScissors` | `on_scissors_button_clicked() -> post_event(EV_MOVE_SCISSORS)` | `src/ui/ui.cpp:76-81` |
  | `btnPlayAgain` | `on_play_again_button_clicked() -> post_event(EV_PLAY_AGAIN)` | `src/ui/ui.cpp:83-88` |
  | `btnRetry` | `on_retry_button_clicked() -> post_event(EV_RETRY)` | `src/ui/ui.cpp:90-95` |
  | `lblStatus*` | `ui_set_status()` | `src/ui/ui.cpp:170-178` |
  | `barSearch` | `ui_set_search_progress()` | `src/ui/ui.cpp:180-186` |
  | `lblTimeout` | `ui_set_search_timeout()` | `src/ui/ui.cpp:188-191` |
  | Move buttons | `ui_set_move_buttons_enabled()` | `src/ui/ui.cpp:193-205` |
  | Screen transitions | `ui_show_screen_*()` family | `src/ui/ui.cpp:114-168` |

  The integration layer provides the single forwarding callback:

  ```cpp
  // src/integration/integration.cpp:33-41
  static void on_ui_event(app::Event event)
  {
      app::sm_post_event(event);
  }

  void integration_init(void)
  {
      ui_register_event_post_callback(on_ui_event);
  }
  ```

  `ui_register_event_post_callback()` is the only registration point; `src/ui/ui.cpp` stores the callback and invokes it from each LVGL event handler. The dynamic `lblTimeout` countdown is driven by Firmware Logic (`AppStateMachine::update_search_timeout_display()` calling the weak `ui_set_search_timeout()` adapter), so the binding is satisfied without adding business logic to the glue layer.

- [PASS] **G14.3 — No new business logic was introduced in integration code.**
  `src/integration/integration.cpp` still contains only:

  - `on_ui_event()` — forwards an LVGL event to `app::sm_post_event()`.
  - `integration_init()` — registers the forwarder.
  - `integration_update()` — calls `app::sm_instance().update()`.

  `src/main.cpp` still contains only hardware initialization, the LVGL display/touch setup, BLE init ordering, and the `loop()` sequence. No game rules, move evaluation, BLE role negotiation, scoring, timeout computation, address-type handling, connection retry logic, manufacturer-data encoding, or state-transition decisions are present in either file.

  The v0.1.9 connection-establishment fix (instrumentation logs, guard delays around advertising stop/connect, HOST advertising restart, `deleteAllBonds()`, and the corrected `status=13` mapping) lives entirely in `src/state_machine/ble_service.cpp` and `src/state_machine/ble_service.h`.

- [PASS] **G14.4 — Thread/task safety is maintained between LVGL task, state-machine task, and hardware ISRs.**
  - LVGL runs in the Arduino `loop()` task. `lv_timer_handler()` is called first; its input-device read callbacks and button-click event handlers execute in that task context.
  - The button-click handlers (`src/ui/ui.cpp:48-95`) call `post_event()`, which invokes the integration-registered callback and ultimately `app::sm_post_event()`.
  - `app::sm_post_event()` (`src/state_machine/app_state_machine.cpp:36-38`) calls `AppStateMachine::enqueue()`, which takes a FreeRTOS mutex created in the `AppStateMachine` constructor (`src/state_machine/app_state_machine.cpp:79`).
  - `integration_update()` runs after `lv_timer_handler()` in the same `loop()` task. It calls `AppStateMachine::update()` (`src/state_machine/app_state_machine.cpp:99-124`), which drives non-blocking BLE work, dispatches deferred discovery, updates the search-timeout label, and drains queued events under the same mutex (`src/state_machine/app_state_machine.cpp:126-138`).
  - A dedicated FreeRTOS BLE radio task is created on physical hardware (`src/state_machine/ble_service.cpp:163-180`) and runs `radio_task_loop()` on core 0. All NimBLE scan/advertise/connect work executes on that task stack.
  - NimBLE callbacks only capture state and post events via `sm_post_event()`, which takes the mutex. They do not mutate `GameContext` or call LVGL directly.
  - State-machine actions run only on the `loop()` task while dispatching, so LVGL adapter calls (`ui_show_screen_*`, `ui_set_status`, etc.) are made from the LVGL-owning task.

  The v0.1.9 fix does not change this topology. The new `vTaskDelay()` calls are inside `BleService::become_host()` and `BleService::connect_to_host()`, which run on the dedicated radio task, not the LVGL or state-machine update context.

- [PASS] **G14.5 — `src/main.cpp` exists and `pio run` succeeds.**
  - `src/main.cpp` exists at `projects/CYD_RPS/src/main.cpp`.
  - `pio run -e esp32-2432s028r_cyd2usb` succeeded.
  - `pio run -e esp32-2432s028r_cyd2usb_wokwi` succeeded.

---

## Build Verification

| Environment | Command | Result |
|-------------|---------|--------|
| Physical CYD2USB | `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** (Flash: 971,009 bytes / 1,310,720; RAM: 76,036 bytes / 327,680) |
| Wokwi simulation | `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** (Flash: 884,049 bytes / 1,310,720; RAM: 68,648 bytes / 327,680) |

Both builds completed without errors or warnings that would block the review.

---

## Additional Checks

### `loop()` Ordering

`src/main.cpp:132-133` calls `lv_timer_handler()` before `integration_update()`:

```cpp
lv_timer_handler();
integration_update();
```

This ordering guarantees that `EV_PLAY` and other LVGL-posted events are fully dispatched before any deferred NimBLE radio work runs, keeping NimBLE radio work outside the LVGL timer context.

### BLE Initialization Order

`src/main.cpp:98-113` initializes BLE in the required order:

```cpp
app::HalService& hal = app::hal_service();
#if defined(WOKWI_SIMULATION)
    Serial.println("CYD_RPS: Wokwi simulation - BLE stack skipped");
#else
    if (!app::ble_service().init()) {
        hal.mark_ble_init_failed(true);
    } else {
        app::ble_service().start_radio_task();
    }
#endif
hal.mark_initialized(!hal.ble_init_failed() && !hal.hw_init_failed());
```

On physical hardware:

1. `ble_service().init()` initializes NimBLE, clears stale bonds, creates the GATT server and move characteristic, starts the service, and starts the GATT server while no GAP procedure is active.
2. `ble_service().start_radio_task()` then creates the dedicated radio task, after the GATT server is already started.

### v0.1.9 BLE Fix Isolation

The v0.1.9 change is confined to Firmware Logic:

- `BleService::init()` calls `NimBLEDevice::deleteAllBonds()` (guarded by `WOKWI_SIMULATION`) to clear stale bonding/identity data.
- `BleService::on_peer_found()`, `resolve_role()`, `become_host()`, `become_join()`, and `connect_to_host()` add instrumentation logs for the connection-establishment flow.
- `BleService::become_host()` stops scanning, stops advertising, waits 20 ms, then restarts advertising to enter a clean peripheral-only connectable state.
- `BleService::connect_to_host()` waits 50 ms after `stop_advertising()` before each `cli->connect()` attempt, avoiding the advertising-teardown race.
- `ble_service.h` corrects the `status=13` comment to identify it as `BLE_ERR_REM_USER_CONN_TERM`.

No integration, UI, or `main.cpp` files were changed for this fix.

### Default PlatformIO Environment

`platformio.ini:12` sets:

```ini
default_envs = esp32-2432s028r_cyd2usb
```

The default environment targets physical CYD2USB hardware. The Wokwi environment (`esp32-2432s028r_cyd2usb_wokwi`) remains available as an explicit non-default target.

---

## Conclusion

All Stage 14 quality gates pass for CYD_RPS v0.1.9. The integration wiring respects ownership boundaries, implements the full UI-to-state-machine contract, introduces no business logic, and preserves task safety with the dedicated BLE radio task. The v0.1.9 BLE connection-establishment fix is confined to Firmware Logic and does not affect the integration layer. `src/main.cpp` exists and the firmware builds successfully for both the physical-hardware and Wokwi environments.

**Final Verdict: GO**
