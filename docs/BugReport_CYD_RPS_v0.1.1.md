# Bug Report: CYD_RPS v0.1.1 — Device resets when Play is pressed; PeerSearch screen never appears

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.1 |
| **Previous bug** | `docs/bug_report_ble_timeout.md` (v0.1.0 BLE timeout / wrong-default-environment issue) |
| **Workflow ID** | wvc_20260617_042659 |
| **Severity** | High — core one-tap start flow is unreachable on physical hardware |
| **Status** | Open — root cause under investigation; serial crash signature not yet captured |
| **Reported** | 2026-06-17 |

## 1. Summary

After flashing the CYD_RPS v0.1.1 physical-hardware build to a CYD2USB v3 board, the firmware boots successfully and the start screen (`SCR_Start`) is rendered. As soon as the user taps **Play**, the device resets and returns to `SCR_Start`. The peer-search screen (`SCR_PeerSearch`) is never shown, and the 10-second single-player fallback is never reached.

This is a *new* failure mode that appears after the v0.1.0 timeout bug was addressed. The v0.1.0 issue was that the wrong PlatformIO environment was flashed, so NimBLE was never initialized and the timeout logic never ran. In v0.1.1 the correct physical environment is used, NimBLE initializes, but the act of *starting* discovery now triggers a reboot.

## 2. Reproduction Steps

1. Build/flash `projects/CYD_RPS` with the default physical-hardware environment:
   ```bash
   python execution/flash_cyd2usb.py --project CYD_RPS --port COM5
   ```
   (The v0.1.1 `platformio.ini` default is `esp32-2432s028r_cyd2usb`.)
2. Reset the board and wait for `SCR_Start` with the **Play** button.
3. Tap **Play**.
4. Observe that the display returns to `SCR_Start` instead of showing `SCR_PeerSearch`.

## 3. Expected Behavior

Per `docs/definition.md` §2 and §3 (UA-02 / UA-06):

> *Start BLE discovery with 10 s timeout, reset game state, switch to `SCR_PeerSearch`.*  
> *If a second device is not discovered within a fixed timeout, the firmware falls back to single-player mode.*

The UI should load `SCR_PeerSearch` with status `"Searching for peer..."`, progress bar at 0%, and `"10s remaining"`. With no peer nearby, it should transition to `SCR_Gameplay` within ~10 seconds.

## 4. Observed Behavior

- The serial log shows the UI event:
  ```text
  UI: Play button clicked
  ```
- `SCR_PeerSearch` is not rendered; no `"Searching for peer..."` status is visible.
- The device resets and reboots to `SCR_Start`.
- The exact reset reason (`Guru Meditation Error`, `Task watchdog`, `Brownout`, or `rst:0x...`) has not yet been captured.

## 5. Root-Cause Analysis

The crash is triggered inside `BleService::start_discovery()`, which is called synchronously from the `EV_PLAY` state-machine action. Several interacting defects make this path unsafe:

### 5.1 `start_discovery()` is invoked from inside the LVGL / state-machine dispatch context

The call chain from a touch release is:

`loop()` → `lv_timer_handler()` → `on_play_button_clicked()` → `post_event(EV_PLAY)` → `integration_update()` → `sm_instance().update()` → `dispatch_pending()` → `dispatch(EV_PLAY)` → `on_play_button()` → `ble_service().start_discovery()`.

`src/state_machine/app_state_machine.cpp` (lines 198–204):

```cpp
void AppStateMachine::on_play_button() {
    ctx_.reset_game();
    ui_show_screen_peer_search();
    ui_set_status("Searching for peer...");
    ui_set_search_progress(0);
    ble_service().start_discovery();
}
```

Starting NimBLE scanning and advertising from deep inside the LVGL timer path means BLE host-task callbacks can preempt the same call stack that is still processing the original UI event. This is a fragile context for blocking or multi-task radio operations.

### 5.2 `start_discovery()` starts both scanning and advertising back-to-back and ignores return values

`src/state_machine/ble_service.cpp` (lines 147–162):

```cpp
    scan->setAdvertisedDeviceCallbacks(new RpsAdvertisedDeviceCallbacks());
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(50);
    scan->start(0, nullptr, false);

    advertising_ = NimBLEDevice::getAdvertising();
    NimBLEAdvertising* adv = adv_ptr(advertising_);
    adv->addServiceUUID(RPS_SERVICE_UUID_STR);
    adv->start();
```

- `NimBLEScan::start()` and `NimBLEAdvertising::start()` both return `bool`, but neither result is checked.
- If either call fails (e.g., `BLE_HS_EBUSY`, host not synced, or controller rejecting concurrent scan+advertise), the code still sets `discovering_ = true` and pretends discovery is running.
- The NimBLE host task may fire advertisement callbacks immediately; those callbacks run on a different FreeRTOS task than the main loop (see `NimBLEDevice::host_task()`).

### 5.3 State-machine event queue is not thread-safe

`src/state_machine/app_state_machine.cpp` (lines 83–91):

```cpp
void AppStateMachine::enqueue(Event ev) {
    size_t next = (queue_tail_ + 1) % EVENT_QUEUE_SIZE;
    if (next == queue_head_) {
        overflow_ = true;
        return;
    }
    event_queue_[queue_tail_] = ev;
    queue_tail_ = next;
}
```

`enqueue()` is called from `sm_post_event()`, which is invoked from NimBLE callbacks (`on_peer_found`, `on_connected`, `on_disconnected`, `on_move_received`) running on the BLE host task. The main task can be inside `dispatch_pending()` updating `queue_head_` at the same time. With no mutex or critical section, the ring buffer can be corrupted, leading to a crash on the next dispatch.

### 5.4 Concurrent scan + advertise may not be supported in this NimBLE/ESP32 configuration

The discovery design relies on every device both scanning and advertising simultaneously (`docs/definition.md` §1 and §4). On some ESP32 NimBLE builds the controller returns an error or even asserts when asked to run both GAP procedures at once. Because the return values are ignored and the state machine assumes discovery is active, any controller-level failure manifests as a reset rather than a graceful transition to the error state.

### 5.5 Power / RF brownout cannot be ruled out without logs

Activating the BLE radio for both scan and advertise simultaneously creates a current spike. A marginal USB power source can brown out the ESP32, producing a reset that looks identical to a software crash. The serial boot log after the reset will show `rst:0x4` or `Brownout` if this is the cause.

## 6. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.cpp` | `start_discovery()` starts scan+advertise synchronously, ignores `bool` return values, and posts events from NimBLE callbacks without queue synchronization. |
| `src/state_machine/app_state_machine.cpp` | `on_play_button()` calls `ble_service().start_discovery()` directly from a dispatched action; `enqueue()`/`dispatch_pending()` are not thread-safe. |
| `src/integration/integration.cpp` | `integration_update()` drives state-machine dispatch immediately after `lv_timer_handler()`, so BLE operations run nested under the UI event context. |
| `src/ui/ui.cpp` | `on_play_button_clicked()` posts `EV_PLAY` from the LVGL event handler with no deferral. |
| `src/main.cpp` | `loop()` calls `lv_timer_handler()` and `integration_update()` back-to-back with no separation between UI and BLE work. |

## 7. Recommended Fixes / Next Steps

1. **Capture the exact crash signature**  
   Open the serial monitor at 115200 baud, tap **Play**, and record the reset reason. This distinguishes software panic/watchdog from a brownout.

2. **Check return values in `start_discovery()`**  
   If `scan->start()` or `adv->start()` returns false, stop discovery and post `Event::EV_ERROR` so the state machine transitions to `Error`/`Start` instead of crashing or silently stalling.

3. **Defer BLE discovery out of the LVGL event context**  
   In `on_play_button()`, set a flag (e.g., `ctx_.pending_discovery = true`) and return. Start discovery on the next `update()` tick from the main loop, after `lv_timer_handler()` has completed.

4. **Make the event queue thread-safe**  
   Protect `enqueue()` / `dispatch_pending()` with a FreeRTOS mutex or critical section because `sm_post_event()` is called from the NimBLE host task.

5. **Reconsider simultaneous scan + advertise**  
   Consider starting only one GAP procedure at a time during peer discovery, then switching to the Host/Join-specific procedure after role resolution (the `become_host()`/`become_join()` paths already stop the unneeded side).

6. **Rule out power issues**  
   Test with a different USB cable/port and check the post-reset boot log for brownout markers.

## 8. Workaround

None known that preserves the intended one-tap flow. If the reset is caused by the synchronous scan+advertise start, a temporary workaround is to flash the Wokwi simulation build (`--env esp32-2432s028r_cyd2usb_wokwi`), but that skips NimBLE entirely and is not useful for physical multiplayer or radio testing.

## 9. Validation Needed

After a fix is implemented, verify on physical hardware:

1. Flash the corrected firmware.
2. Reset and tap **Play**.
3. Confirm `SCR_PeerSearch` appears and remains stable for at least 10 seconds.
4. Confirm the single-player fallback reaches `SCR_Gameplay` with status `"No peer found — single player"`.
5. Confirm a move can be selected and the result screen is shown.
