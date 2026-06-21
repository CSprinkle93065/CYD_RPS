# Assessment: Stage 14 — Integration Code Critic

**Workflow ID:** wvc_20260618_011606  
**Project:** CYD_RPS  
**Target Version:** 0.2.1  
**Board:** CYD2USB v3 (ESP32-2432S028R)  
**Review Date:** 2026-06-21

**Verdict:** GO

---

## Findings

- [PASS] **G14.1 — Integration Agent modified NO files owned by Display & Input or Firmware Logic agents.**

  The Integration Agent owns `src/integration/*` and the top-level `src/main.cpp`. A `git diff` of those files relative to the v0.2.0 baseline is empty, confirming that the integration layer itself was not changed in this Stage 13 pass.

  The working-tree modifications in `src/ui/*` and `src/state_machine/*` are the Stage 9/11 Display & Input and Firmware Logic bug-fix outputs (U01–U05 and B01) that were already reviewed and approved by Stage 10 (`docs/ui_code_review.md`) and Stage 12 (`docs/state_machine_code_review.md`). They are not integration-agent modifications and therefore do not violate the ownership boundary.

- [PASS] **G14.2 — All contract bindings are implemented.**

  Every binding in `docs/state_machine_contract.json` is wired:

  | Contract control / adapter | Required event / adapter call | Implementation location |
  |----------------------------|-------------------------------|--------------------------|
  | `btnHostGame` | `post_event(EV_HOST_GAME)` | `src/ui/ui.cpp:85-90` |
  | `btnSolo` (SCR_Start) | `post_event(EV_SOLO)` | `src/ui/ui.cpp:92-97` |
  | `btnCancel` | `post_event(EV_CANCEL_HOST)` | `src/ui/ui.cpp:99-104` |
  | `btnRetry` (HostTimeoutDialog) | `post_event(EV_HOST_RETRY)` | `src/ui/ui.cpp:106-111` |
  | `btnSolo` (HostTimeoutDialog) | `post_event(EV_SOLO)` | `src/ui/ui.cpp:113-118` |
  | `btnRock` | `post_event(EV_MOVE_ROCK)` | `src/ui/ui.cpp:120-125` |
  | `btnPaper` | `post_event(EV_MOVE_PAPER)` | `src/ui/ui.cpp:127-132` |
  | `btnScissors` | `post_event(EV_MOVE_SCISSORS)` | `src/ui/ui.cpp:134-139` |
  | `btnPlayAgain` | `post_event(EV_PLAY_AGAIN)` | `src/ui/ui.cpp:141-146` |
  | `btnRetry` (SCR_Error) | `post_event(EV_RETRY)` | `src/ui/ui.cpp:148-153` |
  | `btnHome` | `post_event(EV_RESET)` | `src/ui/ui.cpp:155-160` |
  | Screen transitions | `ui_show_screen_*()` | `src/ui/ui.cpp:211-278` |
  | `lblStatus` | `ui_set_status()` | `src/ui/ui.cpp:280-288` |
  | `lblDeviceId` | `ui_set_device_id()` | `src/ui/ui.cpp:290-297` |
  | `lblScore` | `ui_set_score()` | `src/ui/ui.cpp:305-316` |
  | `barProgress` | `ui_set_host_wait_progress()` | `src/ui/ui.cpp:318-325` (called from `AppStateMachine::update_host_wait_progress()`) |
  | Move buttons | `ui_set_move_buttons_enabled()` | `src/ui/ui.cpp:327-339` |
  | `serial_HOST` | `EV_HOST_GAME` | `src/state_machine/serial_command_handler.cpp:91-92` |
  | `serial_SOLO` | `EV_SOLO` | `src/state_machine/serial_command_handler.cpp:93-94` |
  | `serial_ROCK` | `EV_MOVE_ROCK` | `src/state_machine/serial_command_handler.cpp:95-96` |
  | `serial_PAPER` | `EV_MOVE_PAPER` | `src/state_machine/serial_command_handler.cpp:97-98` |
  | `serial_SCISSORS` | `EV_MOVE_SCISSORS` | `src/state_machine/serial_command_handler.cpp:99-100` |
  | `serial_AGAIN` | `EV_PLAY_AGAIN` | `src/state_machine/serial_command_handler.cpp:101-102` |
  | `serial_RESET` / `serial_HOME` | `EV_RESET` | `src/state_machine/serial_command_handler.cpp:103-104` |

  The integration glue registers one forwarder and one periodic hook:

  ```cpp
  // src/integration/integration.cpp:32-45
  static void on_ui_event(app::Event event)
  {
      app::sm_post_event(event);
  }

  void integration_init(void)
  {
      ui_register_event_post_callback(on_ui_event);
  }

  void integration_update(void)
  {
      app::sm_instance().update();
  }
  ```

- [PASS] **G14.3 — No new business logic was introduced in integration code.**

  `src/integration/integration.cpp` contains only event forwarding and the state-machine update call. `src/main.cpp` contains only the single-owner hardware initialization sequence, subsystem wiring, BLE init ordering, the `setup()` flow, and the `loop()` schedule. Neither file implements game rules, move evaluation, scoring, BLE role negotiation, connection retry logic, timeout computation, manufacturer-data parsing, or state-transition decisions.

- [PASS] **G14.4 — Thread/task safety is maintained between LVGL task, state-machine task, and hardware ISRs.**

  - LVGL runs in the Arduino `loop()` task. `ui_handler()` (`src/main.cpp:99`) calls `lv_timer_handler()` first; its input-device read callbacks and button-click handlers execute in that task context.
  - Button-click handlers call the integration-registered callback, which calls `app::sm_post_event()`. `AppStateMachine::enqueue()` takes a FreeRTOS mutex created in the constructor (`src/state_machine/app_state_machine.cpp:77-82`).
  - `integration_update()` runs after `ui_handler()` in the same `loop()` task. `AppStateMachine::update()` drains queued events under the same mutex (`src/state_machine/app_state_machine.cpp:87-98, 136-153`).
  - A dedicated FreeRTOS BLE radio task is created on physical hardware (`src/state_machine/ble_service.cpp:202-226`, pinned to core 0). NimBLE scan/advertise/connect work executes on that task stack.
  - NimBLE callbacks only capture state and post events via `sm_post_event()`; they do not mutate `GameContext` or call LVGL directly.
  - UI adapter calls (`ui_show_screen_*`, `ui_set_status`, etc.) are made from state-machine actions while dispatching on the LVGL-owning `loop()` task.

- [PASS] **G14.5 — `src/main.cpp` exists and `pio run` succeeds.**

  - `src/main.cpp` exists at `projects/CYD_RPS/src/main.cpp`.
  - `pio run -e esp32-2432s028r_cyd2usb` completed successfully.
  - `pio run -e esp32-2432s028r_cyd2usb_wokwi` completed successfully.

---

## Build Verification

| Environment | Command | Result |
|-------------|---------|--------|
| Physical CYD2USB | `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** — Flash: 953,033 / 1,310,720 bytes; RAM: 76,148 / 327,680 bytes |
| Wokwi simulation | `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** — Flash: 865,605 / 1,310,720 bytes; RAM: 68,760 / 327,680 bytes |

Both builds completed without errors.

---

## Additional Checks

### `loop()` Ordering

`src/main.cpp:96-100` calls LVGL before the state-machine update:

```cpp
ui_handler();
integration_update();
```

This keeps NimBLE radio work outside the `lv_timer_handler()` context.

### BLE Initialization Order

`src/main.cpp:61-78` initializes NimBLE and starts the dedicated radio task only after the GATT server has been started during `ble_service().init()`, satisfying the LL-044 sequencing requirement.

### Serial Command Integration

`src/main.cpp:102-104` calls the serial command parser and dispatcher from `loop()`; command parsing and event posting remain in `src/state_machine/serial_command_handler.cpp`.

---

## Conclusion

All Stage 14 quality gates pass for CYD_RPS v0.2.1. The integration wiring respects agent ownership boundaries, implements every UI and serial binding declared in `docs/state_machine_contract.json`, introduces no business logic, preserves task safety with the dedicated BLE radio task and mutex-protected event queue, and builds successfully for both physical and Wokwi environments.

**Final Verdict: GO**
