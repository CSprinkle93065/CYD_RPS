# Bug Report: CYD_RPS v0.1.7 — Multiplayer still fails; JOIN stops advertising before HOST discovers it

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.7 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.6.md` (JOIN advertising while connecting caused `status=13`) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | Open |
| **Reported** | 2026-06-19 |

---

## 1. Summary

v0.1.7 attempted to fix the v0.1.6 `status=13` failure by:

- Stopping the JOIN's advertising before `NimBLEClient::connect()`.
- Reducing `NimBLEClient::connect()` timeout from 30 s to 5 s.
- Restarting advertising if all retries failed.

After flashing v0.1.7 to two CYD2USB v3 boards, the JOIN side still cannot establish a multiplayer link. The failure mode changed from one 30-second timeout to four 5-second timeouts, but the underlying `status=13` (`BLE_ERR_CONN_REJ_RESOURCES`) remains. The new evidence shows the JOIN stops advertising too early: the peer that should become HOST never discovers the JOIN, never resolves its own role, and is still scanning when the JOIN tries to connect.

---

## 2. Environment

- **Board:** ESP32-2432S028R CYD2USB v3
- **Firmware:** CYD_RPS v0.1.7 (970,093 bytes flash, built from `main` after GitHub release)
- **Units:**
  - Unit A (COM6): public MAC `d4:e9:f4:af:42:a0`
  - Unit B (COM5): public MAC `d4:e9:f4:a9:8d:90`
- **Serial:** COM5 and COM6 at 115200 baud, captured simultaneously with `.tmp/dual_serial_logger.py`
- **Test setup:** Both units flashed with `projects/CYD_RPS/.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`.

---

## 3. Reproduction Steps

1. Flash v0.1.7 to both boards.
2. Reset both boards.
3. Tap **Play** on Unit A (COM6).
4. Tap **Play** on Unit B (COM5) within a few seconds.
5. Observe both screens and the dual-serial log.

---

## 4. Expected Behavior

1. Both units enter `SCR_PeerSearch` and perform concurrent scan + advertise.
2. Each unit discovers the other.
3. Role resolution uses the public MAC ordering: lower MAC (`d4:e9:f4:a9:8d:90`) becomes HOST, higher MAC (`d4:e9:f4:af:42:a0`) becomes JOIN.
4. The HOST stops scanning and continues advertising.
5. The JOIN stops scanning, keeps advertising long enough for the HOST to discover it, then stops advertising and initiates the connection.
6. After the link is established, both units reach multiplayer `SCR_Gameplay`.

---

## 5. Observed Behavior

Dual-serial log (excerpt, timestamps from the v0.1.7 field test):

```text
[14:54:58] COM5> UI: Play button clicked
[14:54:58] COM5> BLE: start_discovery heap=173224
[14:54:58] COM5> BLE: start_advertising heap=173148
[14:54:58] COM6> UI: Play button clicked
[14:54:58] COM6> BLE: start_discovery heap=173220
[14:54:58] COM6> BLE: start_advertising heap=173144
[14:54:59] COM6> I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
[14:54:59] COM6> I NimBLEScan: Updated advertiser: d4:e9:f4:a9:8d:92
...
[14:55:04] COM6> E NimBLEClient: Connection failed; status=13
[14:55:04] COM6> BLE: connect attempt 1/4 failed
[14:55:09] COM6> E NimBLEClient: Connection failed; status=13
[14:55:09] COM6> BLE: connect attempt 2/4 failed
[14:55:14] COM6> E NimBLEClient: Connection failed; status=13
[14:55:14] COM6> BLE: connect attempt 3/4 failed
[14:55:19] COM6> E NimBLEClient: Connection failed; status=13
[14:55:19] COM6> BLE: connect attempt 4/4 failed
[14:55:19] COM6> ERROR: BLE join connection failed after retries
[14:55:19] COM6> BLE: start_advertising heap=170528
```

Key observations:

- **COM6 (higher MAC) discovered COM5 at 14:54:59** and became JOIN.
- **COM5 (lower MAC) never logged a discovery of COM6**; it remained in `PeerSearch` scanning throughout the attempt.
- **COM6 stopped advertising before connecting.** Once it began the first `connect()` attempt at 14:55:04, COM5 had no further opportunity to discover it.
- **All four connect attempts failed with `status=13`.** The 5-second timeout change only made the retries faster; it did not remove the resource conflict.
- **After all retries, COM6 restarted advertising.** By then the 10-second discovery window on COM5 may have already expired.

The screen on COM6 remained on **"Connecting"** until the error screen appeared after ~20 seconds.

---

## 6. Decoded Failure Location

| Function | File | Role |
|----------|------|------|
| `BleService::become_join()` | `src/state_machine/ble_service.cpp` | Configures JOIN role; intentionally keeps advertising (v0.1.6 fix). |
| `BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | Stops advertising and initiates connection (v0.1.7 fix). |
| `BleService::start_advertising()` / `stop_advertising()` | `src/state_machine/ble_service.cpp` | Control JOIN discoverability. |
| `RpsAdvertisedDeviceCallbacks::onResult()` | `src/state_machine/ble_service.cpp` | HOST-side discovery callback; never saw JOIN after JOIN stopped advertising. |
| `BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Resolves roles, but HOST can only resolve after discovering JOIN. |

---

## 7. Root-Cause Analysis

### 7.1 The JOIN advertises too briefly for the HOST to discover it

v0.1.7 added `stop_advertising()` at the start of `connect_to_host()`. As soon as the JOIN resolves its role (which happens the moment it discovers the HOST), it transitions to `Connecting` and calls `connect_to_host()`. In the observed test, only ~1 second elapsed between COM6 discovering COM5 and COM6 stopping its advertisement. COM5 never discovered COM6 in that window.

BLE discovery is probabilistic: the scanner listens only during its scan window (50 ms every 100 ms in the current configuration), and the advertiser transmits on its own interval. In a noisy RF environment, a 1-second advertising window is not sufficient for reliable mutual discovery.

### 7.2 The HOST cannot stop scanning until it discovers the JOIN

The HOST role is resolved inside `resolve_role()`, which is triggered only when `on_peer_found()` receives a peer advertisement. Because COM5 never received COM6's advertisement, COM5 never called `become_host()` and never stopped scanning. When COM6 tried to connect to COM5, COM5's controller was still actively scanning, contributing to the `BLE_ERR_CONN_REJ_RESOURCES` (`status=13`) rejection.

### 7.3 The 5-second connect timeout is a symptom, not the root cause

Reducing the connect timeout from 30 s to 5 s made the failure surface faster, but every attempt still failed because the peer was not ready. The root cause is a timing/race condition in the role-negotiation handshake, not the timeout value.

### 7.4 Restarting advertising only after all retries is too late

v0.1.7 restarts advertising inside `connect_to_host()` only after all four retries fail. By that point the HOST's 10-second discovery timeout may already have fired, or the user has already seen the error screen.

---

## 8. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.h` | `kConnectRetryDelayMs`, `kConnectTimeoutSeconds`, `kJoinPreConnectDelayMs` (if added). |
| `src/state_machine/ble_service.cpp` | `become_join()` keeps advertising; `connect_to_host()` stops advertising before connect and retries. |
| `src/state_machine/app_state_machine.cpp` | May need a state or timer to keep JOIN advertising before transitioning to `Connecting`. |
| `docs/state_machine.puml` | May need an explicit `RoleNegotiating` wait state if the fix is state-machine driven. |

---

## 9. Recommended Fix Directions

1. **Give the HOST a guaranteed discovery window.** After the JOIN resolves its role, keep advertising for a bounded time (e.g., 2 seconds) before stopping advertising and initiating the connection. This can be implemented either:
   - As a blocking delay in `connect_to_host()` before `stop_advertising()` (simplest, but blocks the radio task), or
   - As a non-blocking timer in `BleService::update()` or a new `RoleNegotiating` wait state in the state machine.

2. **Restart advertising between connect retries.** On each failed attempt (except the last), restart advertising, wait a backoff long enough for one advertising interval plus scan window (e.g., 1.5–2 seconds), then stop advertising and try again. This gives the HOST multiple discovery windows without requiring a long initial delay.

3. **Consider a shorter advertising interval.** Reduce the NimBLE advertisement interval during peer search so the HOST is more likely to hear the JOIN within the discovery window. This is secondary to the advertising-window fix.

4. **Optional: embed role intent in the advertisement.** Once roles are resolved, the JOIN could stop advertising the RPS service and the HOST could continue. However, this still requires the HOST to discover the JOIN at least once, so it does not replace the discovery-window fix.

---

## 10. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB v3 boards.
2. Start both units with various timing offsets (simultaneous, A first, B first).
3. Confirm via dual-serial log that:
   - The HOST discovers the JOIN before the JOIN stops advertising.
   - The HOST logs `BLE: start_advertising` after role resolution (or remains advertising) and stops scanning.
   - The JOIN logs a discovery-window wait and does not log `status=13`.
   - Both units post `EV_CONNECTED` and reach multiplayer `SCR_Gameplay`.
4. Confirm the fallback to single-player still works when only one unit is present.


---

## 11. Post-Report Research Notes

### 11.1 `status=13` is a NimBLE connect timeout, not an HCI resource error

Inspection of `NimBLEClient.cpp` in the `release/1.4` branch shows the connect path:

```cpp
void NimBLEClient::setConnectTimeout(uint8_t time) {
    m_connectTimeout = (uint32_t)(time * 1000);
}
```

and in `connect()`:

```cpp
rc = ble_gap_connect(NimBLEDevice::m_own_addr_type, &peerAddr_t,
                     m_connectTimeout, &m_pConnParams,
                     NimBLEClient::handleGapEvent, this);
```

If the controller reports a connect event with a non-zero status, `connect()` logs:

```text
E NimBLEClient: Connection failed; status=%d %s
```

The decimal value `13` matches `BLE_HS_ETIMEOUT` in the NimBLE host return-code namespace, for which `NimBLEUtils::returnCodeToString()` returns `"Operation timed out."`. This is consistent with NimBLE-Arduino issue #915, where the same log sequence appears next to `"Connect timeout - cancelling"`.

So `status=13` means the peer did not respond to the connection request before the connect timeout expired. Earlier reports that mapped this directly to the HCI code `BLE_ERR_CONN_REJ_RESOURCES` were imprecise; the practical symptom is the same (the peer is not accepting a connection), but the reported mechanism is a timeout.

### 11.2 The 5-second retry spacing is exactly `setConnectTimeout(5)`

Because `setConnectTimeout()` takes whole seconds and multiplies by 1000, the v0.1.7 call `setConnectTimeout(5)` sets a 5000 ms connect timeout. That matches the observed ~5-second interval between `status=13` messages. In v0.1.6 the default 30-second timeout produced one failure every ~30 seconds. Reducing the timeout only surfaces the same failure faster; it does not remove the cause.

### 11.3 A scanning ESP32 is a poor connection target

The ESP32 BLE controller cannot reliably accept an incoming connection while it is actively scanning. The NimBLE/ESP32 community notes:

> "it is highly advised to stop scan when you are trying to connect to peripheral and advertising should be stopped by low level API"
> — esp32.com, *BLE peripheral and central simultaneously*

The ESP-IDF `blecent` example also follows the sequence:

1. Stop advertising.
2. Scan for the target.
3. Stop scanning.
4. Initiate the connection.

In the v0.1.7 test, COM5 (intended HOST) never resolved its role because it never discovered COM6 after COM6 stopped advertising. Because COM5 never stopped scanning, it could not accept COM6's connection request, so COM6's `connect()` eventually timed out with `status=13`.

### 11.4 Revised failure chain

1. Both units start scan + advertise.
2. COM6 discovers COM5, resolves as JOIN, and stops advertising.
3. COM5 has only a short window of COM6 advertisements to detect and misses them.
4. COM5 stays in scan mode and never becomes HOST.
5. COM6 tries to connect to a peer that is still scanning → no response → `status=13` timeout.
6. The 5-second timeout/retry cycle repeats four times.
7. Only after all retries does COM6 restart advertising, by which time COM5's peer-search window may have expired.

### 11.5 Implications for the fix

The fix must satisfy **two** independent conditions before the JOIN connects:

- **Discovery:** the HOST must see at least one JOIN advertisement so it can resolve to HOST and stop scanning.
- **Acceptance:** the HOST must actually stop scanning, because an ESP32 that is scanning cannot reliably accept a connection.

Therefore:

- Keeping the JOIN advertising for a guaranteed window after role resolution is necessary.
- The JOIN should not stop advertising until it is confident the HOST has discovered it.
- A robust pattern is: **keep advertising while waiting, stop advertising only immediately before `connect()`, and on each timeout failure restart advertising for a short discovery window before the next attempt.** This repeatedly gives the HOST new chances to discover and stop scanning, without relying on a single long initial delay.
- Shortening the advertising interval during peer search would increase the probability that the HOST discovers the JOIN within each window.

### 11.6 References

- `NimBLEClient.cpp` release/1.4 (raw source): `setConnectTimeout(uint8_t time)` and `connect()` implementation.
- `NimBLEUtils.cpp` release/1.4 (raw source): `returnCodeToString()` mapping `BLE_HS_ETIMEOUT` to `"Operation timed out."`.
- h2zero/NimBLE-Arduino issue #915 — log excerpt showing `Connection failed; status=13` together with "Connect timeout".
- esp32.com topic *"BLE peripheral and central simultaneously"* — recommendation to stop scan when connecting and to let the low-level API stop advertising.
- Espressif `esp-idf/examples/bluetooth/nimble/blecent` — standard sequence: stop advertising → scan → stop scan → connect.
