# Assessment: Stage 14 — Integration Code Critic (re-review)

**Workflow ID:** wvc_20260617_171607  
**Project:** CYD_RPS  
**Version:** 0.2.0  
**Revision Type:** minor_revision  
**Board:** CYD2USB v3 (ESP32-2432S028R)  
**Review Date:** 2026-06-21

**Verdict:** GO

---

## Findings

- [PASS] **G14.1 — Integration Agent modified NO files owned by Display & Input or Firmware Logic agents.**

  The Integration Agent owns `src/integration/*` and the top-level `src/main.cpp`. Working-tree changes attributable to the integration re-run are limited to those integration-owned files:

  - `src/integration/integration.cpp`
  - `src/integration/integration.h`
  - `src/main.cpp`

  No modifications were made by the Integration Agent to files under `src/ui/*` (Display & Input ownership) or `src/state_machine/*` (Firmware Logic ownership). The substantial changes observed in `src/ui/*` and `src/state_machine/*` are part of the v0.2.0 Display & Input / Firmware Logic agent deliverables and are outside the integration agent's scope.

- [PASS] **G14.2 — All contract bindings are implemented.**

  `docs/state_machine_contract.json` declares the v0.2.0 control bindings. All clickable controls post their assigned events, all state-driven UI adapter callbacks have strong implementations, and the serial command bindings are wired.

  | Control / Adapter | Contract Action | Implementation |
  |-------------------|-----------------|----------------|
  | `btnHostGame` | `on_host_game_button_clicked() -> post_event(EV_HOST_GAME)` | `src/ui/ui.cpp:77-82` |
  | `btnSolo` (SCR_Start) | `on_solo_button_clicked() -> post_event(EV_SOLO)` | `src/ui/ui.cpp:84-89` |
  | `btnCancel` | `on_cancel_host_button_clicked() -> post_event(EV_CANCEL_HOST)` | `src/ui/ui.cpp:91-96` |
  | `btnRetry` (HostTimeoutDialog) | `on_host_retry_button_clicked() -> post_event(EV_HOST_RETRY)` | `src/ui/ui.cpp:98-103` |
  | `btnSolo` (HostTimeoutDialog) | `on_host_solo_button_clicked() -> post_event(EV_SOLO)` | `src/ui/ui.cpp:105-110` |
  | `btnRock` | `on_rock_button_clicked() -> post_event(EV_MOVE_ROCK)` | `src/ui/ui.cpp:112-117` |
  | `btnPaper` | `on_paper_button_clicked() -> post_event(EV_MOVE_PAPER)` | `src/ui/ui.cpp:119-124` |
  | `btnScissors` | `on_scissors_button_clicked() -> post_event(EV_MOVE_SCISSORS)` | `src/ui/ui.cpp:126-131` |
  | `btnPlayAgain` | `on_play_again_button_clicked() -> post_event(EV_PLAY_AGAIN)` | `src/ui/ui.cpp:133-138` |
  | `btnRetry` (SCR_Error) | `on_retry_button_clicked() -> post_event(EV_RETRY)` | `src/ui/ui.cpp:140-145` |
  | `btnHome` | `on_home_button_clicked() -> post_event(EV_RESET)` | `src/ui/ui.cpp:147-152` |
  | Screen transitions | `ui_show_screen_*()` family | `src/ui/ui.cpp:203-267` |
  | `lblStatus` | `ui_set_status()` | `src/ui/ui.cpp:269-277` |
  | `lblDeviceId` | `ui_set_device_id()` | `src/ui/ui.cpp:279-286` |
  | `lblScore` | `ui_set_score()` | `src/ui/ui.cpp:294-300` |
  | `barProgress` | `ui_set_host_wait_progress()` | `src/ui/ui.cpp:302-309` |
  | Move buttons | `ui_set_move_buttons_enabled()` | `src/ui/ui.cpp:311-323` |
  | `serial_HOST` | `serial_command_dispatch() -> EV_HOST_GAME` | `src/state_machine/serial_command_handler.cpp:91-92` |
  | `serial_SOLO` | `serial_command_dispatch() -> EV_SOLO` | `src/state_machine/serial_command_handler.cpp:93-94` |
  | `serial_ROCK` | `serial_command_dispatch() -> EV_MOVE_ROCK` | `src/state_machine/serial_command_handler.cpp:95-96` |
  | `serial_PAPER` | `serial_command_dispatch() -> EV_MOVE_PAPER` | `src/state_machine/serial_command_handler.cpp:97-98` |
  | `serial_SCISSORS` | `serial_command_dispatch() -> EV_MOVE_SCISSORS` | `src/state_machine/serial_command_handler.cpp:99-100` |
  | `serial_AGAIN` | `serial_command_dispatch() -> EV_PLAY_AGAIN` | `src/state_machine/serial_command_handler.cpp:101-102` |
  | `serial_RESET` / `serial_HOME` | `serial_command_dispatch() -> EV_RESET` | `src/state_machine/serial_command_handler.cpp:103-104` |

  The integration layer provides the single forwarding callback:

  ```cpp
  // src/integration/integration.cpp:32-40
  static void on_ui_event(app::Event event)
  {
      app::sm_post_event(event);
  }

  void integration_init(void)
  {
      ui_register_event_post_callback(on_ui_event);
  }
  ```

  `ui_register_event_post_callback()` is the sole registration point. The dynamic `barProgress` countdown is driven by Firmware Logic (`AppStateMachine::update_host_wait_progress()` calling the weak `ui_set_host_wait_progress()` adapter), so the binding is satisfied without adding business logic to the glue layer.

- [PASS] **G14.3 — No new business logic was introduced in integration code.**

  `src/integration/integration.cpp` contains only:

  - `on_ui_event()` — forwards an LVGL event to `app::sm_post_event()`.
  - `integration_init()` — registers the forwarder.
  - `integration_update()` — calls `app::sm_instance().update()`.

  `src/main.cpp` contains only hardware initialization, subsystem wiring, BLE init ordering, the `setup()` sequence, and the `loop()` task schedule. No game rules, move evaluation, BLE role negotiation, scoring, timeout computation, address-type handling, connection retry logic, manufacturer-data encoding, or state-transition decisions are present in either file.

- [PASS] **G14.4 — Thread/task safety is maintained between LVGL task, state-machine task, and hardware ISRs.**

  - LVGL runs in the Arduino `loop()` task. `ui_handler()` (which calls `lv_timer_handler()`) is called first; its input-device read callbacks and button-click event handlers execute in that task context.
  - The button-click handlers (`src/ui/ui.cpp:77-152`) call `post_event()`, which invokes the integration-registered callback and ultimately `app::sm_post_event()`.
  - `app::sm_post_event()` calls `AppStateMachine::enqueue()`, which takes a FreeRTOS mutex created in the `AppStateMachine` constructor (`src/state_machine/app_state_machine.cpp:77-81`).
  - `integration_update()` runs after `ui_handler()` in the same `loop()` task. It calls `AppStateMachine::update()` (`src/state_machine/app_state_machine.cpp:100-109`), which drives non-blocking BLE work, updates the host-wait progress bar, and drains queued events under the same mutex (`src/state_machine/app_state_machine.cpp:136-153`).
  - A dedicated FreeRTOS BLE radio task is created on physical hardware in `ble_service.cpp` and runs on core 0. All NimBLE scan/advertise/connect work executes on that task stack.
  - NimBLE callbacks only capture state and post events via `sm_post_event()`, which takes the mutex. They do not mutate `GameContext` or call LVGL directly.
  - State-machine actions run only on the `loop()` task while dispatching, so LVGL adapter calls (`ui_show_screen_*`, `ui_set_status`, etc.) are made from the LVGL-owning task.

- [PASS] **G14.5 — `src/main.cpp` exists and `pio run` succeeds.**

  - `src/main.cpp` exists at `projects/CYD_RPS/src/main.cpp`.
  - `pio run -e esp32-2432s028r_cyd2usb` succeeded.
  - `pio run -e esp32-2432s028r_cyd2usb_wokwi` succeeded.

---

## Build Verification

| Environment | Command | Result |
|-------------|---------|--------|
| Physical CYD2USB | `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** (Flash: 952,225 bytes / 1,310,720; RAM: 76,140 bytes / 327,680) |
| Wokwi simulation | `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** (Flash: 864,833 bytes / 1,310,720; RAM: 68,752 bytes / 327,680) |

Both builds completed without errors or warnings that would block the review.

---

## Additional Checks

### `loop()` Ordering

`src/main.cpp:96-100` calls `ui_handler()` (LVGL) before `integration_update()`:

```cpp
ui_handler();
integration_update();
```

This ordering guarantees that `EV_HOST_GAME` and other LVGL-posted events are fully dispatched before any deferred NimBLE radio work runs, keeping NimBLE radio work outside the LVGL timer context.

### BLE Initialization Order

`src/main.cpp:61-78` initializes BLE in the required order:

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

1. `ble_service().init()` initializes NimBLE and the GATT server while no GAP procedure is active.
2. `ble_service().start_radio_task()` then creates the dedicated radio task, after the GATT server is already started.

### Serial Command Integration

`src/main.cpp:102-103` calls the Firmware Logic serial command handlers from `loop()`:

```cpp
app::serial_process_input();
app::serial_command_dispatch();
```

This is pure wiring: command parsing and event posting live in `src/state_machine/serial_command_handler.cpp`.

---

## Conclusion

All Stage 14 quality gates pass for CYD_RPS v0.2.0 on re-review. The integration wiring respects ownership boundaries, implements the full UI-to-state-machine contract including the serial command bindings, introduces no business logic, and preserves task safety with the dedicated BLE radio task. `src/main.cpp` exists and the firmware builds successfully for both the physical-hardware and Wokwi environments.

**Final Verdict: GO**
