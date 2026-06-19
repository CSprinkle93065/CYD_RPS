# Assessment: Stage 14 — Integration Code Critic

**Project:** CYD_RPS  
**Version:** 0.1.7 (bug-fix revision)  
**Workflow ID:** wvc_20260619_134606  
**Reviewer:** Integration Code Critic  
**Review Date:** 2026-06-19  
**Verdict:** GO

---

## Quality-Gate Results

| Gate | Criterion | Result |
|------|-----------|--------|
| G14.1 | Integration Agent modified NO files owned by Display & Input or Firmware Logic agents. | PASS |
| G14.2 | All contract bindings are implemented. | PASS |
| G14.3 | No new business logic was introduced in integration code. | PASS |
| G14.4 | Thread/task safety is maintained between LVGL task, state-machine task, and hardware ISRs. | PASS |
| G14.5 | `src/main.cpp` exists and `pio run` succeeds. | PASS |

## Summary

The CYD_RPS v0.1.7 bug-fix revision addresses the BLE multiplayer JOIN connection failure (`status=13` / `BLE_ERR_CONN_REJ_RESOURCES`) documented in `docs/BugReport_CYD_RPS_v0.1.6.md` §8. The fix is confined to Firmware Logic: `src/state_machine/ble_service.cpp` and `src/state_machine/ble_service.h`. Stage 13 confirmed that no integration wiring changes were required.

The integration layer (`src/integration/integration.cpp`, `src/integration/integration.h`) and the application entry point (`src/main.cpp`) remain unchanged glue code. They forward LVGL input events to the state-machine event queue and run the state-machine/BLE update hook after `lv_timer_handler()` returns, keeping NimBLE radio work outside the LVGL timer context. No agent-ownership boundaries were violated, all contract bindings remain wired, no business logic leaked into integration code, and both PlatformIO environments build successfully.

---

## Detailed Findings

### G14.1 — Ownership Boundaries

Integration-owned files:

- `projects/CYD_RPS/src/integration/integration.cpp`
- `projects/CYD_RPS/src/integration/integration.h`
- `projects/CYD_RPS/src/main.cpp`

Display & Input owned files (`src/ui/*`) and Firmware Logic owned files (`src/state_machine/*`) show no integration-specific edits for this revision:

- The Stage 12 Firmware Logic Code Critic review confirms the v0.1.7 change is isolated to `src/state_machine/ble_service.cpp` and `src/state_machine/ble_service.h`.
- `#include "integration/integration.h"` appears only in `src/main.cpp` and `src/integration/integration.cpp`; no UI or state-machine file depends on the integration module.
- The UI layer still exposes the single registration point `ui_register_event_post_callback()` and overrides the weak UI adapter callbacks only in `src/ui/ui.cpp`.
- The state-machine layer still exposes `app::sm_post_event()` and `app::sm_instance().update()`; it does not depend on the integration module.

**Result:** PASS

### G14.2 — Contract Bindings

All controls and state-driven updates declared in `docs/state_machine_contract.json` remain wired:

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

The integration layer registers the single forwarding callback in `src/integration/integration.cpp:33-41`:

```cpp
static void on_ui_event(app::Event event)
{
    app::sm_post_event(event);
}

void integration_init(void)
{
    ui_register_event_post_callback(on_ui_event);
}
```

The dynamic `lblTimeout` countdown is driven by Firmware Logic (`AppStateMachine::update_search_timeout_display()` calling `ui_set_search_timeout()`). The integration layer does not poll or compute dynamic values (LL-041).

**Result:** PASS

### G14.3 — No New Business Logic in Integration Code

- `src/integration/integration.cpp` contains exactly:
  - one event-forwarding stub (`on_ui_event`),
  - `integration_init()` registering that stub,
  - `integration_update()` calling `app::sm_instance().update()`.
- No game rules, move evaluation, BLE role negotiation, scoring, timeout computation, address-type handling, connection retry logic, manufacturer-data encoding, or state-transition decisions are present.
- `src/main.cpp` is system initialization and `loop()` ordering only. The `#ifdef WOKWI_SIMULATION` NimBLE skip is an environment-specific hardware-probe decision, not application business logic.
- The v0.1.7 BLE fixes live entirely in `src/state_machine/ble_service.cpp`/`ble_service.h`.

**Result:** PASS

### G14.4 — Thread/Task Safety

- LVGL runs in the Arduino `loop()` task. `lv_timer_handler()` is called first; its input-device read callbacks and button-click event handlers execute in that task context.
- The button-click handlers (`src/ui/ui.cpp:48-95`) call `post_event()`, which invokes the integration-registered callback and ultimately `app::sm_post_event()`.
- `app::sm_post_event()` (`src/state_machine/app_state_machine.cpp:82-84`) enqueues events under a FreeRTOS mutex created in the `AppStateMachine` constructor (`src/state_machine/app_state_machine.cpp:79`).
- `integration_update()` runs after `lv_timer_handler()` in the same `loop()` task. It calls `AppStateMachine::update()` (`src/state_machine/app_state_machine.cpp:99-124`), which:
  1. drives non-blocking BLE work (`ble_service().update()`),
  2. starts deferred discovery if `ctx_.start_discovery_pending` is set,
  3. updates the search-timeout label,
  4. dispatches queued events under the same mutex (`src/state_machine/app_state_machine.cpp:126-138`).
- A dedicated FreeRTOS BLE radio task is created on physical hardware (`src/state_machine/ble_service.cpp:152-176`) and pinned to core 0. It consumes commands from a `QueueHandle_t` and performs NimBLE scan, advertise, role-resolution, and connection work on its own stack, keeping NimBLE off the LVGL/main-loop stack (LL-039, LL-042).
- NimBLE callbacks (BLE host task) only capture state and post events via `sm_post_event()`, which takes the mutex. They do not mutate `GameContext` or call LVGL directly (LL-040).
- State-machine actions run only on the `loop()` task while dispatching, so LVGL adapter calls (`ui_show_screen_*`, `ui_set_status`, etc.) are made from the LVGL-owning task.
- Hardware is initialized exactly once in `src/main.cpp:53-119`; state-machine init actions are probes only (`src/state_machine/app_state_machine.cpp:44-61`) (LL-034).

**Result:** PASS

### G14.5 — Build

- `src/main.cpp` exists at `projects/CYD_RPS/src/main.cpp`.
- Build commands executed:

```bash
pio run -e esp32-2432s028r_cyd2usb
pio run -e esp32-2432s028r_cyd2usb_wokwi
```

- Results:
  - `esp32-2432s028r_cyd2usb`: SUCCESS (RAM 23.2%, Flash 74.0%)
  - `esp32-2432s028r_cyd2usb_wokwi`: SUCCESS (RAM 20.9%, Flash 67.4%)

**Result:** PASS

---

## Additional Checks

### `loop()` Ordering

`src/main.cpp:122-135` calls `lv_timer_handler()` **before** `integration_update()`:

```cpp
lv_timer_handler();
integration_update();
```

This satisfies the requirement that deferred BLE discovery runs after the UI event has been dispatched, keeping NimBLE radio work out of the LVGL timer context (LL-039).

### BLE Initialization Order

`src/main.cpp:97-113` initializes BLE in the required order:

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

1. `ble_service().init()` initializes NimBLE, creates the GATT server and move characteristic, starts the service, and starts the GATT server while no GAP procedure is active. This preserves the v0.1.4 `BLE_HS_EBUSY`/`abort()` fix (LL-044).
2. `ble_service().start_radio_task()` then creates the dedicated radio task, after the GATT server is already started.

### v0.1.7 BLE Fix Isolation

The v0.1.7 change is confined to Firmware Logic:

- `BleService::connect_to_host()` stops advertising before initiating the connection (`src/state_machine/ble_service.cpp:433`), with an in-file comment citing `docs/BugReport_CYD_RPS_v0.1.6.md` §8.5.
- `BleService::kConnectTimeoutSeconds` is set to 5 s and applied via `cli->setConnectTimeout(kConnectTimeoutSeconds)` (`src/state_machine/ble_service.cpp:446`), reducing the per-attempt timeout from the NimBLE 30 s default.
- On retry exhaustion, `connect_to_host()` restarts advertising and posts `EV_ERROR` (`src/state_machine/ble_service.cpp:464-473`).

No integration or UI files were changed for this fix.

### Default PlatformIO Environment

`platformio.ini:12` sets:

```ini
default_envs = esp32-2432s028r_cyd2usb
```

The default environment targets physical CYD2USB hardware. The Wokwi environment (`esp32-2432s028r_cyd2usb_wokwi`) remains available as an explicit non-default target (LL-038).

### Host State-Machine Tests

`tests/host/test_state_machine.py` was executed and all 40 `host_assert` tests pass, confirming the generated dispatch table still matches the documented transitions.

---

## Conclusion

All Stage 14 quality gates pass for CYD_RPS v0.1.7. The integration wiring respects ownership boundaries, implements the full UI-to-state-machine contract, introduces no business logic, and preserves task safety with the dedicated BLE radio task. The v0.1.7 BLE multiplayer fixes are confined to Firmware Logic and do not affect the integration layer. `src/main.cpp` exists and the firmware builds successfully for both the physical-hardware and Wokwi environments.
