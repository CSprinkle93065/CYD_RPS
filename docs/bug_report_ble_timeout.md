# Bug Report: CYD_RPS BLE peer-search never times out to single-player fallback

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.0 |
| **Workflow ID** | wvc_20260617_044037 |
| **Severity** | High — core game-flow single-player fallback is unreachable |
| **Status** | Open — root cause identified, no code fix applied yet |
| **Reported** | 2026-06-17 |

## 1. Summary

After flashing CYD_RPS to a physical CYD2USB v3 board, the firmware boots and the start screen renders correctly. When the user taps **Play**, the peer-search screen appears but never transitions to the single-player (automated opponent) fallback after the expected 10-second BLE discovery timeout. The device remains stuck in peer search indefinitely.

## 2. Reproduction Steps

1. Build and flash `projects/CYD_RPS` using the workspace flash helper without specifying a PlatformIO environment:
   ```bash
   python execution/flash_cyd2usb.py --project CYD_RPS --port COM5
   ```
2. Reset the board and wait for the start screen.
3. Tap **Play**.
4. Wait > 10 seconds without a second RPS peer nearby.

## 3. Expected Behavior

Per `docs/definition.md` §1 and §3 (UA-06):

> *If a second device is not discovered within a fixed timeout, the firmware falls back to single-player mode and plays against a uniform random-number-generator opponent.*

The UI should switch to the gameplay screen with status `"No peer found — single player"` within ~10 seconds.

## 4. Observed Behavior

- The device remains on `SCR_PeerSearch`.
- The progress bar stays at 0% and the countdown label stays at `"10s remaining"`.
- No automated opponent/single-player fallback is triggered.
- Serial output shows the UI "Play button clicked" event but no `"No peer found"` status or gameplay transition.

## 5. Root-Cause Analysis

The most likely immediate cause is a **build-vs-runtime mismatch**: the default PlatformIO environment is the Wokwi simulation build, and the flash helper selected that build when no `--env` argument was supplied.

### 5.1 Default environment points at the Wokwi build

`projects/CYD_RPS/platformio.ini`:

```ini
[platformio]
default_envs = esp32-2432s028r_cyd2usb_wokwi
```

`execution/flash_cyd2usb.py::get_build_env()` returns `default_envs` when `--env` is omitted, so the command above flashes the Wokwi binary to physical hardware.

### 5.2 Wokwi build skips BLE initialization

`projects/CYD_RPS/src/main.cpp` (lines 96–108):

```cpp
#if defined(WOKWI_SIMULATION)
    // Wokwi's ESP32 emulator does not emulate NimBLE reliably ... Skip BLE stack init
    Serial.println("CYD_RPS: Wokwi simulation - BLE stack skipped");
#else
    if (!app::ble_service().init()) {
        hal.mark_ble_init_failed(true);
    }
#endif
```

In the Wokwi binary, `BleService::initialized_` is never set to `true`.

### 5.3 `start_discovery()` silently returns when BLE is not initialized

`projects/CYD_RPS/src/state_machine/ble_service.cpp` (lines 120–149):

```cpp
void BleService::start_discovery() {
    if (!initialized_) {
        return;                 // <-- silent early return, no state change
    }

    role_ = Role::NONE;
    connected_ = false;
    discovering_ = true;        // never reached if !initialized_
    peer_timeout_posted_ = false;
    ...
    discovery_start_ms_ = millis();
}
```

Because `start_discovery()` returns early, `discovering_` remains `false` and `discovery_start_ms_` remains `0`.

### 5.4 Timeout logic is gated by `discovering_`

`projects/CYD_RPS/src/state_machine/ble_service.cpp` (lines 311–320):

```cpp
void BleService::post_peer_timeout_if_needed(uint32_t now_ms) {
    if (!discovering_ || peer_timeout_posted_ || role_ != Role::NONE) {
        return;
    }
    if (now_ms - discovery_start_ms_ >= DISCOVERY_TIMEOUT_MS) {
        peer_timeout_posted_ = true;
        stop_discovery();
        sm_post_event(Event::EV_PEER_TIMEOUT);
    }
}
```

Since `discovering_` is `false`, the timeout function returns immediately on every `loop()` iteration. The state machine therefore never receives `EV_PEER_TIMEOUT`, and the `PeerSearch --> SinglePlayerFallback` transition is never taken.

## 6. Additional Risk: Concurrent advertise/scan may self-discover

Even with the correct hardware build, the current discovery strategy starts both advertising and scanning simultaneously (`ble_service.cpp` lines 134–146). On some NimBLE/ESP32 configurations the local radio can report its own advertisement with an address that does not compare equal to `NimBLEDevice::getAddress()`. If that happens:

- `on_peer_found()` resolves a role (HOST or JOIN) and sets `role_ != Role::NONE`.
- `post_peer_timeout_if_needed()` then suppresses the timeout because of the `role_ != Role::NONE` guard.
- The firmware may attempt to connect to itself and remain stuck in `RoleNegotiating`/`Connecting`.

This is a secondary, unconfirmed risk; the primary cause above matches the observed symptom when a Wokwi binary is flashed to hardware.

## 7. Affected Files

| File | Role in bug |
|------|-------------|
| `projects/CYD_RPS/platformio.ini` | Default environment is `esp32-2432s028r_cyd2usb_wokwi`, causing physical flashes to use the simulation binary. |
| `projects/CYD_RPS/src/main.cpp` | Wokwi build skips `ble_service().init()`. |
| `projects/CYD_RPS/src/state_machine/ble_service.cpp` | `start_discovery()` silently returns if not initialized; timeout is gated by `discovering_`. |
| `execution/flash_cyd2usb.py` | Selects `platformio.ini`'s `default_envs` when `--env` is omitted. |

## 8. Recommended Fixes

1. **Change the default PlatformIO environment to the physical-hardware build**, or require an explicit environment selection in `flash_cyd2usb.py` for physical flashes.
2. **Make `BleService::start_discovery()` fail loudly** when not initialized — log an error and post `Event::EV_ERROR` so the state machine transitions to `Error`/`Start` instead of silently stalling in `PeerSearch`.
3. **Add a simulation-specific single-player smoke path** (e.g., under `WOKWI_SIMULATION`, immediately post `EV_PEER_TIMEOUT` from `start_discovery()`) so the Wokwi build can still exercise the fallback flow.
4. **Add a self-discovery guard** in `on_peer_found()` that ignores any advertisement whose address matches the local public address regardless of address-type representation, and consider throttling duplicate own-ad detections.

## 9. Workaround

Flash the physical-hardware environment explicitly:

```bash
python execution/flash_cyd2usb.py --project CYD_RPS --env esp32-2432s028r_cyd2usb --port COM5
```

This builds/flashes a binary that initializes NimBLE, so `BleService::start_discovery()` will set `discovering_ = true` and the 10-second timeout will fire as designed.

## 10. Validation Needed

After a fix is implemented, verify on physical hardware:

1. Flash with the corrected environment.
2. Tap **Play** with no peer nearby.
3. Confirm transition to gameplay and status `"No peer found — single player"` within ~10 seconds.
4. Confirm a move can be selected and the result screen shows a random opponent move.
