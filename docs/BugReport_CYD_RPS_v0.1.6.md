# Bug Report: CYD_RPS v0.1.6 — Multiplayer still fails; JOIN gets stuck in "Connecting"

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.6 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.5.md` (asymmetric JOIN/HOST role negotiation) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | **Fixed in v0.1.7** |
| **Reported** | 2026-06-19 |

---

## 1. Summary

v0.1.6 attempted to fix the v0.1.5 asymmetric role-negotiation failure by:

- Keeping the JOIN role advertising during role negotiation,
- Embedding the public MAC in BLE advertisement manufacturer data for symmetric role resolution,
- Adding a bounded JOIN→HOST connection retry, and
- Preventing the HOST from restarting an active advertisement.

After flashing v0.1.6 to two CYD2USB v3 boards, the serial-connected unit still fails to establish a multiplayer link. It discovers the peer, transitions to **"Connecting"**, and remains there indefinitely. No `EV_CONNECTED` is posted and no further BLE state changes appear on the serial log. A later dual-serial capture (Section 8) confirmed the JOIN side fails with NimBLE status `13` (`BLE_ERR_CONN_REJ_RESOURCES`) because the JOIN continues advertising while attempting to connect.

---

## 2. Environment

- **Board:** ESP32-2432S028R CYD2USB v3
- **Firmware:** CYD_RPS v0.1.6 (976,496 bytes)
- **Units:**
  - Unit A: `d4:e9:f4:af:42:a0`
  - Unit B: `d4:e9:f4:a9:8d:90`
- **Serial monitor:** COM5, 115200 baud
- **Test setup:** Both units flashed with `projects/CYD_RPS/.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`.

---

## 3. Reproduction Steps

1. Flash v0.1.6 to both boards.
2. Reset both boards.
3. Tap **Play** on Unit A.
4. Tap **Play** on Unit B within a few seconds.
5. Observe both screens.

---

## 4. Expected Behavior

1. Both units enter `SCR_PeerSearch` and perform concurrent scan + advertise.
2. Both units discover each other.
3. Role resolution uses the public MAC ordering: lower MAC becomes HOST, higher MAC becomes JOIN.
4. The HOST stops scanning and continues advertising.
5. The JOIN stops scanning, keeps advertising, and initiates the connection.
6. After the link is established, both units reach multiplayer `SCR_Gameplay`.

---

## 5. Observed Behavior

Serial log captured from the unit on COM5 (Unit B, MAC `d4:e9:f4:a9:8d:90`):

```text
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
CYD_RPS setup start
I NimBLEDevice: BLE Host Task Started
I NimBLEDevice: NimBle host synced.
CYD_RPS setup done
TOUCH raw=(1987,1953) screen=(122,155) z=1441
UI: Play button clicked
BLE: start_discovery heap=173224
BLE: start_advertising_if_needed heap=173148
BLE: start_advertising heap=173148
I NimBLEScan: New advertiser: 02:75:6c:e1:45:d9
I NimBLEScan: New advertiser: 4b:63:13:db:8a:95
...
I NimBLEScan: New advertiser: d4:e9:f4:af:42:a2
I NimBLEScan: Updated advertiser: d4:e9:f4:af:42:a2
BLE: start_advertising heap=169776
```

After the final `BLE: start_advertising heap=169776` line, the log produces no further output. The screen on the serial-connected unit remains on **"Connecting"**.

> **Update:** A subsequent dual-serial capture of both units (see Section 8) showed the same symptom and revealed the underlying failure: the JOIN side reports `E NimBLEClient: Connection failed; status=13` on every connect attempt.

The other unit (Unit A) was not on a serial monitor, so its exact state is unknown.

---

## 6. Decoded Failure Location

| Function | File | Role |
|----------|------|------|
| `BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Determines HOST vs JOIN. |
| `BleService::become_join()` | `src/state_machine/ble_service.cpp` | JOIN configuration and connection trigger. |
| `BleService::become_host()` | `src/state_machine/ble_service.cpp` | HOST configuration. |
| `BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | JOIN central attempts the BLE link. |
| `RpsAdvertisedDeviceCallbacks::onResult()` | `src/state_machine/ble_service.cpp` | Parses peer public MAC from manufacturer data. |
| `BleService::start_advertising()` | `src/state_machine/ble_service.cpp` | Builds advertisement including manufacturer data. |

---

## 7. Root-Cause Analysis (Hypotheses)

### 7.1 Public-MAC manufacturer data may not be parsed correctly

v0.1.6 relies on `advertisedDevice->getManufacturerData()` containing the peer public MAC prefixed by a test company ID (`0xFFFF`). If the data is not present, malformed, or parsed with the wrong byte order, `resolve_role()` falls back to the random advertisement address. Random addresses are not guaranteed to have the same ordering as public MACs, which can cause both units to select the same role or the wrong role.

### 7.2 Role assignment may be wrong on one or both sides

The serial-connected unit (Unit B, public MAC `d4:e9:f4:a9:8d:90`) should become HOST because its public MAC is lower than Unit A (`d4:e9:f4:af:42:a0`). If it instead became JOIN, it would explain the "Connecting" screen. Without logs from both units, we cannot confirm whether:

- Unit B resolved as JOIN and is trying to connect, while Unit A never resolved as HOST, or
- Both units resolved as JOIN and are both trying to connect to a peer that is not acting as HOST.

### 7.3 The JOIN connection attempt may be hanging

`connect_to_host()` calls `NimBLEClient::connect()` up to four times with a 250 ms backoff. If the HOST is not ready or not advertising connectably, each `connect()` call may block for several seconds. The "Connecting" screen could persist for the entire retry window without visible serial output. The absence of "BLE: connect attempt X/Y failed" messages suggests the attempt has not yet completed or no retry logging is being emitted.

### 7.4 The HOST may not be entering HOST mode

Even if the JOIN keeps advertising, the peer may not be discovering it, or may discover it but fail to resolve/transition to HOST. Possible causes:

- The HOST's scan window misses the JOIN's advertisement.
- The HOST's `become_host()` restarts advertising in a way that changes its address.
- The HOST's discovery timeout fires before it sees the JOIN.

---

## 8. Updated Empirical Findings (dual-serial capture, 2026-06-19)

A simultaneous capture of both units was performed with the dual-serial logger after Section 5 was written.

### 8.1 Setup

- **COM5:** Unit A — MAC `d4:e9:f4:af:42:a0` (USB-SERIAL CH340).
- **COM6:** Unit B — MAC `d4:e9:f4:a9:8d:90` (USB-SERIAL CH340).
- Both units running CYD_RPS v0.1.6.
- Unit B (COM6) tapped **Play** first; Unit A (COM5) tapped **Play** ~1 second later.

### 8.2 Role resolution

```text
[13:37:06] COM6> UI: Play button clicked
[13:37:06] COM6> BLE: start_discovery heap=173224
[13:37:06] COM6> BLE: start_advertising heap=173148
...
[13:37:07] COM5> UI: Play button clicked
[13:37:07] COM5> BLE: start_discovery heap=173220
[13:37:07] COM5> BLE: start_advertising heap=173144
[13:37:07] COM5> I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
[13:37:07] COM5> I NimBLEScan: Updated advertiser: d4:e9:f4:a9:8d:92
[13:37:07] COM6> I NimBLEScan: New advertiser: d4:e9:f4:af:42:a2
[13:37:07] COM6> I NimBLEScan: Updated advertiser: d4:e9:f4:af:42:a2
[13:37:07] COM6> BLE: start_advertising heap=170480
```

COM6 (Unit B, lower public MAC) became HOST and continued advertising. COM5 (Unit A, higher public MAC) became JOIN. Role resolution was symmetric and correct.

### 8.3 Connection failure

The JOIN unit (COM5) then entered `connect_to_host()` and failed as follows:

```text
[13:37:37] COM5> E NimBLEClient: Connection failed; status=13
[13:37:37] COM5> BLE: connect attempt 1/4 failed
[13:38:07] COM5> E NimBLEClient: Connection failed; status=13
[13:38:07] COM5> BLE: connect attempt 2/4 failed
[13:38:37] COM5> E NimBLEClient: Connection failed; status=13
[13:38:37] COM5> BLE: connect attempt 3/4 failed
[13:39:08] COM5> E NimBLEClient: Connection failed; status=13
[13:39:08] COM5> BLE: connect attempt 4/4 failed
[13:39:08] COM5> ERROR: BLE join connection failed after retries
```

Key observations:

- `status=13` is `BLE_ERR_CONN_REJ_RESOURCES` from the ESP32 NimBLE controller.
- Each attempt took exactly 30 seconds, matching `NimBLEClient::connect()`'s default connect timeout. The 250 ms `kConnectRetryDelayMs` backoff is effectively hidden by the 30-second blocking call.
- The HOST (COM6) never logged an incoming connection and continued advertising.
- The JOIN (COM5) never stopped advertising before initiating the connection (v0.1.6 intentionally keeps JOIN advertising during role negotiation).

### 8.4 Impact on hypotheses

- **Hypothesis 7.1 (manufacturer-data parsing):** Unlikely. Both units parsed the peer public MAC correctly and resolved roles symmetrically.
- **Hypothesis 7.2 (wrong role assignment):** Disconfirmed. Each unit resolved the same roles; COM6/HOST, COM5/JOIN.
- **Hypothesis 7.3 (hanging retry):** Partially correct about long delays, but the root cause is `status=13`, not a missing/unready HOST.
- **Hypothesis 7.4 (HOST not entering HOST mode):** Disconfirmed. COM6 entered HOST mode and advertised continuously.

### 8.5 Updated primary root cause

The leading cause is now **resource contention on the JOIN side**: the JOIN remains a peripheral advertiser while trying to act as a central and initiate a connection. The ESP32 NimBLE controller rejects the connection with `BLE_ERR_CONN_REJ_RESOURCES` (`status=13`).

The 30-second per-attempt timeout and four retry attempts produce an apparent ~2-minute "Connecting" stall before the JOIN finally posts `EV_ERROR`.

### 8.6 Recommended fix direction

1. **Stop JOIN advertising before connecting.** In `become_join()`, after role resolution, call `stop_advertising()` before invoking `connect_to_host()`.
2. **Restart advertising if the connection fails** (e.g., on `EV_ERROR` or after `connect_to_host()` exhausts retries) so the JOIN can still be discovered if the role-negotiation window is extended or retried.
3. **Consider reducing `NimBLEClient::connect()` timeout** (e.g., `setConnectTimeout(5)`) so a resource-conflict failure is detected quickly instead of blocking for 30 seconds per attempt.
4. **Alternatively, advertise using the public MAC** (`NimBLEDevice::setOwnAddrType(BLE_ADDR_PUBLIC)`) and rely on the random/public address distinction rather than manufacturer data.

## 9. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.h` | `peer_public_addr_`, `kManufacturerCompanyId`, `kConnectRetries`, `kConnectRetryDelayMs`. |
| `src/state_machine/ble_service.cpp` | Manufacturer-data encoding/decoding, role resolution, JOIN/HOST configuration, connection retry. |
| `docs/definition.md` | May need to document the role-resolution algorithm more explicitly. |

---

## 10. Recommended Next Steps

1. **Stop JOIN advertising before connecting.** Modify `become_join()` so the JOIN stops advertising before `connect_to_host()` runs. Verify the HOST still advertises and the JOIN no longer receives `status=13`.
2. **Restart advertising on connection failure.** If `connect_to_host()` exhausts retries, return the JOIN to an advertising state so subsequent role-negotiation attempts are possible.
3. **Reduce connect timeout.** Set `NimBLEClient::setConnectTimeout(5)` (or similar) so a resource-conflict failure surfaces in seconds rather than the default 30 seconds per attempt.
4. **Add persistent role/connect logging.** Print local public MAC, parsed peer public MAC, resolved role, and the result of each `connect()` attempt for future diagnostics.
5. **Evaluate public-address advertising.** Consider forcing NimBLE to advertise with the public MAC (`NimBLEDevice::setOwnAddrType(BLE_ADDR_PUBLIC)`) to remove the manufacturer-data parsing dependency.

---

## 11. Fix Applied in v0.1.7

The following code changes were implemented in `src/state_machine/ble_service.h` and `src/state_machine/ble_service.cpp`:

1. **Stop JOIN advertising before connecting.** `BleService::connect_to_host()` now calls `stop_advertising()` after creating the `NimBLEClient` and before the retry loop. This removes the controller resource conflict that produced `status=13` (`BLE_ERR_CONN_REJ_RESOURCES`).
2. **Reduced connect timeout.** Added `kConnectTimeoutSeconds = 5` and called `cli->setConnectTimeout(kConnectTimeoutSeconds)` before the first `connect()` attempt. This reduces the per-attempt timeout from the NimBLE default of 30 s to 5 s.
3. **Restart advertising on connection failure.** If all retries fail, `connect_to_host()` calls `start_advertising()` before posting `EV_ERROR`, keeping the unit discoverable if the state machine re-enters a discovery/negotiation cycle.
4. **In-file rationale comments** were added next to each change citing `docs/BugReport_CYD_RPS_v0.1.6.md` §8.5–8.6 per the lessons-learned requirement (LL-043).

Build artifacts:

- `projects/CYD_RPS/.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`
- `projects/CYD_RPS/dist/CYD_RPS_v0.1.7.zip`

Host state-machine tests pass. The two-board physical-hardware multiplayer test remains manual and is documented in `docs/qa_results.md` §6.

## 12. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB v3 boards.
2. Start both units with various timing offsets (simultaneous, A first, B first).
3. Confirm both units discover each other and resolve roles symmetrically via serial logs.
4. Confirm the JOIN unit connects successfully (`EV_CONNECTED` posted, no `status=13`, no indefinite "Connecting").
5. Confirm both units reach multiplayer `SCR_Gameplay` and exchange moves.
6. Confirm the fallback to single-player still works when only one unit is present.
