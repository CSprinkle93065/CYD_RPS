# CYD_RPS v0.1.8 — Integration Code Review (Stage 14)

| Field | Value |
|-------|-------|
| **Workflow ID** | wvc_20260619_072600 |
| **Project** | CYD_RPS |
| **Version** | 0.1.8 |
| **Revision Type** | bug_fix |
| **Reviewer** | Integration Code Critic (Stage 14) |
| **Date** | 2026-06-20 |

---

## Verdict: **GO**

The v0.1.8 BLE discovery-window fix is confined to Firmware Logic. The integration layer and `src/main.cpp` were not modified, the existing UI-to-state-machine contract wiring remains intact, no business logic leaked into integration code, task safety is preserved, and both target environments build successfully. This review supersedes the previous Stage 14 integration review for v0.1.7.

---

## Build Verification

| Environment | Command | Result |
|-------------|---------|--------|
| Physical CYD2USB | `pio run -e esp32-2432s028r_cyd2usb` | **SUCCESS** (Flash: 970,205 bytes / 1,310,720; RAM: 76,036 bytes / 327,680) |
| Wokwi simulation | `pio run -e esp32-2432s028r_cyd2usb_wokwi` | **SUCCESS** (Flash: 883,321 bytes / 1,310,720; RAM: 68,648 bytes / 327,680) |

Both builds completed without errors or warnings that would block the review.

---

## Quality Gate Findings

### G14.1 — Integration Agent modified NO files owned by Display & Input or Firmware Logic agents

**Status: PASS**

Working-tree changes for this revision are limited to:

- `context.md` (orchestration context)
- `docs/state_machine_code_review.md` (Stage 12 review artifact)
- `src/state_machine/ble_service.cpp` (Firmware Logic)
- `src/state_machine/ble_service.h` (Firmware Logic)

Integration-owned files are unchanged relative to `HEAD`:

- `src/integration/integration.cpp`
- `src/integration/integration.h`
- `src/main.cpp`

Display & Input owned files under `src/ui/*` are also unchanged:

- `src/ui/ui.cpp`
- `src/ui/ui.h`
- `src/ui/screens.cpp`
- `src/ui/theme.cpp`
- `src/ui/theme.h`
- `src/ui/touch_mapping.cpp`
- `src/ui/touch_mapping.h`
- `src/ui/ui_internal.h`

`git diff HEAD` against these files produced no output, confirming no integration-level edits.

### G14.2 — All contract bindings are implemented

**Status: PASS**

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

`ui_register_event_post_callback()` is the only registration point; `ui.cpp` stores the callback and invokes it from each LVGL event handler. The dynamic `lblTimeout` countdown is driven by Firmware Logic (`AppStateMachine::update_search_timeout_display()` calling the weak `ui_set_search_timeout()` adapter), so the binding is satisfied without adding business logic to the glue layer.

### G14.3 — No new business logic was introduced in integration code

**Status: PASS**

`src/integration/integration.cpp` still contains only:

- `on_ui_event()` — forwards an LVGL event to `app::sm_post_event()`.
- `integration_init()` — registers the forwarder.
- `integration_update()` — calls `app::sm_instance().update()`.

`src/main.cpp` still contains only hardware initialization, the LVGL display/touch setup, BLE init ordering, and the `loop()` sequence `lv_timer_handler()` followed by `integration_update()`. No game rules, move evaluation, BLE role negotiation, scoring, timeout computation, address-type handling, connection retry logic, manufacturer-data encoding, or state-transition decisions are present in either file.

The v0.1.8 discovery-window fix (advertising intervals, JOIN discovery windows, connect retry windows) lives entirely in `src/state_machine/ble_service.cpp` and `src/state_machine/ble_service.h`.

### G14.4 — Thread/task safety is maintained between LVGL task, state-machine task, and hardware ISRs

**Status: PASS**

- LVGL runs in the Arduino `loop()` task. `lv_timer_handler()` is called first; its input-device read callbacks and button-click event handlers execute in that task context.
- The button-click handlers (`src/ui/ui.cpp:48-95`) call `post_event()`, which invokes the integration-registered callback and ultimately `app::sm_post_event()`.
- `app::sm_post_event()` (`src/state_machine/app_state_machine.cpp:36-38`) calls `AppStateMachine::enqueue()`, which takes a FreeRTOS mutex created in the `AppStateMachine` constructor (`src/state_machine/app_state_machine.cpp:79`).
- `integration_update()` runs after `lv_timer_handler()` in the same `loop()` task. It calls `AppStateMachine::update()` (`src/state_machine/app_state_machine.cpp:99-124`), which:
  1. drives non-blocking BLE work (`ble_service().update()`),
  2. starts deferred discovery if `ctx_.start_discovery_pending` is set,
  3. updates the search-timeout label,
  4. dispatches queued events under the same mutex (`src/state_machine/app_state_machine.cpp:126-138`).
- A dedicated FreeRTOS BLE radio task is created on physical hardware (`src/state_machine/ble_service.cpp`) and pinned to core 0. It consumes commands from a `QueueHandle_t` and performs NimBLE scan, advertise, role-resolution, and connection work on its own stack, keeping NimBLE off the LVGL/main-loop stack.
- NimBLE callbacks only capture state and post events via `sm_post_event()`, which takes the mutex. They do not mutate `GameContext` or call LVGL directly.
- State-machine actions run only on the `loop()` task while dispatching, so LVGL adapter calls (`ui_show_screen_*`, `ui_set_status`, etc.) are made from the LVGL-owning task.
- Hardware is initialized exactly once in `src/main.cpp:53-119`; state-machine init actions are probes only (`src/state_machine/app_state_machine.cpp:44-61`).

The v0.1.8 fix does not change this topology. The discovery-window delays and retry windows are handled inside the dedicated radio task or via non-blocking timers in `BleService::update()`, never on the LVGL task or inside integration code.

### G14.5 — `src/main.cpp` exists and `pio run` succeeds

**Status: PASS**

- `src/main.cpp` exists at `projects/CYD_RPS/src/main.cpp`.
- `pio run -e esp32-2432s028r_cyd2usb` succeeded.
- `pio run -e esp32-2432s028r_cyd2usb_wokwi` succeeded.

---

## Additional Checks

### `loop()` Ordering

`src/main.cpp:122-135` calls `lv_timer_handler()` before `integration_update()`:

```cpp
lv_timer_handler();
integration_update();
```

This ordering guarantees that EV_PLAY and other LVGL-posted events are fully dispatched before any deferred NimBLE radio work runs, keeping NimBLE radio work outside the LVGL timer context.

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

1. `ble_service().init()` initializes NimBLE, creates the GATT server and move characteristic, starts the service, and starts the GATT server while no GAP procedure is active.
2. `ble_service().start_radio_task()` then creates the dedicated radio task, after the GATT server is already started.

### v0.1.8 BLE Fix Isolation

The v0.1.8 change is confined to Firmware Logic:

- `BleService::connect_to_host()` keeps advertising during each discovery window and stops advertising only immediately before `cli->connect(addr)`.
- `kJoinDiscoveryWindowMs = 2000` is used before the first connect attempt; `kJoinConnectInterRetryWindowMs = 2000` is used between retries.
- `kPeerSearchAdvMinInterval`/`kPeerSearchAdvMaxInterval` shorten the advertising interval during peer search so the HOST is more likely to discover the JOIN within each window.
- On retry exhaustion, `connect_to_host()` restarts advertising and posts `EV_ERROR`.

No integration, UI, or `main.cpp` files were changed for this fix.

### Default PlatformIO Environment

`platformio.ini:12` sets:

```ini
default_envs = esp32-2432s028r_cyd2usb
```

The default environment targets physical CYD2USB hardware. The Wokwi environment (`esp32-2432s028r_cyd2usb_wokwi`) remains available as an explicit non-default target.

---

## Conclusion

All Stage 14 quality gates pass for CYD_RPS v0.1.8. The integration wiring respects ownership boundaries, implements the full UI-to-state-machine contract, introduces no business logic, and preserves task safety with the dedicated BLE radio task. The v0.1.8 BLE discovery-window fix is confined to Firmware Logic and does not affect the integration layer. `src/main.cpp` exists and the firmware builds successfully for both the physical-hardware and Wokwi environments.

**Final Verdict: GO**
