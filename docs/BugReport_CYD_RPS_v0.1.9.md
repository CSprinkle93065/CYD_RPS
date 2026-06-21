# Bug Report: CYD_RPS v0.1.9 — JOIN discovers HOST, role resolves, but `connect()` still fails with `status=13`

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.9 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.8.md` (connection-establishment race: no guard delay, HOST advertising not restarted, stale bonding) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | Open |
| **Reported** | 2026-06-21 |

---

## 1. Summary

v0.1.9 attempted to fix the v0.1.8 `status=13` failure by:

- Adding a 50 ms guard delay after `stop_advertising()` before `NimBLEClient::connect()`.
- Restarting HOST advertising cleanly after stopping scan in `become_host()`.
- Adding `Serial.println` instrumentation to the discovery, role-resolution, and connection paths.
- Calling `NimBLEDevice::deleteAllBonds()` during `BleService::init()` (guarded for hardware builds).
- Correcting the misleading `status=13` comment in `ble_service.h`.

After flashing v0.1.9 to two CYD2USB v3 boards, both units boot cleanly, both discover the RPS peer, and both resolve roles correctly. However, every JOIN→HOST `connect()` attempt still fails with `status=13`. The failure is now deterministic and identical on all four retries, and the instrumentation shows a suspicious byte-order mismatch between the address reported by `NimBLEScan` and the address used in `NimBLEClient::connect()`.

---

## 2. Environment

- **Board:** ESP32-2432S028R CYD2USB v3
- **Firmware:** CYD_RPS v0.1.9 (GitHub release `v0.1.9` + `CYD_RPS_v0.1.9.zip`)
- **NimBLE-Arduino:** 1.4.3
- **Units (as observed during this test):**
  - Unit on **COM5:** public MAC `d4:e9:f4:a9:8d:90` — lower MAC, resolved **HOST**
  - Unit on **COM6:** public MAC `d4:e9:f4:af:42:a0` — higher MAC, resolved **JOIN**
- **Serial:** COM5 and COM6 at 115200 baud, captured simultaneously with `.tmp/dual_serial_logger.py`
- **Test setup:** Both units reset; COM5 tapped **Play** first, then COM6 tapped **Play** a few seconds later.

---

## 3. Reproduction Steps

1. Flash v0.1.9 to both boards.
2. Reset both boards.
3. Tap **Play** on COM5 (lower MAC / expected HOST).
4. Tap **Play** on COM6 (higher MAC / expected JOIN) within a few seconds.
5. Observe both screens and the dual-serial log.

---

## 4. Expected Behavior

1. Both units enter `SCR_PeerSearch` and perform concurrent scan + advertise.
2. Each unit discovers the other’s RPS-service advertisement.
3. Role resolution uses the public MAC ordering: COM5 lower → HOST; COM6 higher → JOIN.
4. HOST stops scanning and enters a clean connectable peripheral state.
5. JOIN keeps advertising for the discovery window, stops advertising, waits 50 ms, then connects.
6. Link is established and both units reach multiplayer `SCR_Gameplay`.

---

## 5. Observed Behavior

Dual-serial log (`.tmp/cyd_rps_dual_serial_log.txt`, timestamps UTC):

```text
[08:59:47.688] COM6> UI: Play button clicked
[08:59:47.688] COM6> BLE: start_discovery heap=173220
[08:59:47.688] COM6> BLE: start_advertising_if_needed heap=173144
[08:59:47.688] COM6> BLE: start_advertising heap=173144
...
[08:59:48.821] COM6> I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
[08:59:48.823] COM6> BLE: on_peer_found addr=92:8d:a9:f4:e9:d4 type=0 public_valid=1
[08:59:48.824] COM6> BLE: resolve_role local=d4:e9:f4:af:42:a2 peer=92:8d:a9:f4:e9:d4 cmp=16 role=JOIN
[08:59:48.824] COM6> JOIN: host discovered, starting connect window
[08:59:48.824] COM6> BLE: initial discovery window before connect
[08:59:48.825] COM6> BLE: start_advertising heap=170400

[08:59:48.844] COM5> UI: Play button clicked
[08:59:48.844] COM5> BLE: start_discovery heap=173224
[08:59:48.844] COM5> BLE: start_advertising_if_needed heap=173148
[08:59:48.845] COM5> BLE: start_advertising heap=173148
[08:59:48.846] COM5> I NimBLEScan: New advertiser: d4:e9:f4:af:42:a2
[08:59:48.846] COM5> BLE: on_peer_found addr=a2:42:af:f4:e9:d4 type=0 public_valid=1
[08:59:48.847] COM5> BLE: resolve_role local=d4:e9:f4:a9:8d:92 peer=a2:42:af:f4:e9:d4 cmp=-16 role=HOST
[08:59:48.847] COM5> BLE: start_advertising heap=171664
[08:59:48.847] COM5> HOST: stopped scan, advertising ready

[08:59:50.876] COM6> JOIN: advertising stopped, connecting to 92:8d:a9:f4:e9:d4
[08:59:55.880] COM6> E NimBLEClient: Connection failed; status=13
[08:59:55.881] COM6> BLE: connect attempt 1/4 failed
[08:59:55.882] COM6> BLE: connect retry 2/4 - discovery window
[08:59:55.882] COM6> BLE: start_advertising heap=170400
[08:59:57.958] COM6> JOIN: advertising stopped, connecting to 92:8d:a9:f4:e9:d4
[09:00:02.959] COM6> E NimBLEClient: Connection failed; status=13
...
[09:00:17.111] COM6> ERROR: BLE join connection failed after retries
```

Key observations:

- **Both units discover each other and resolve roles correctly.**
  - COM6 higher MAC → `role=JOIN`.
  - COM5 lower MAC → `role=HOST`.
- **HOST logs `HOST: stopped scan, advertising ready`** after stopping scan, stopping advertising, and restarting advertising.
- **JOIN logs the new 50 ms guard delay and the connect attempt**, but every attempt still fails with `status=13`.
- **The connect address is byte-reversed relative to the scan log.**
  - `NimBLEScan` reports the HOST as `d4:e9:f4:a9:8d:92`.
  - The JOIN’s instrumentation prints `connecting to 92:8d:a9:f4:e9:d4`, which is the exact reverse byte order.
- **The 50 ms guard delay did not remove the failure.**
- **All four retries fail identically**, so the failure is deterministic, not a timing race.

---

## 6. Decoded Failure Location

| Function | File | Role in this failure |
|----------|------|----------------------|
| `RpsAdvertisedDeviceCallbacks::onResult()` | `src/state_machine/ble_service.cpp` | Copies peer address bytes with `advertisedDevice->getAddress().getNative()`. |
| `BleService::on_peer_found()` | `src/state_machine/ble_service.cpp` | Stores `peer_addr_` from the callback; logs it with `NimBLEAddress(peer_addr_, peer_addr_type_)`, which prints reversed. |
| `BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Compares public MACs and chooses HOST/JOIN correctly. |
| `BleService::become_host()` | `src/state_machine/ble_service.cpp` | Stops scan, stops advertising, restarts advertising after 20 ms. Logs `HOST: stopped scan, advertising ready`. |
| `BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | Stops advertising, waits 50 ms, reconstructs `NimBLEAddress(addr_buf, peer_addr_type_)`, calls `cli->connect(addr)`. |
| `NimBLEClient::connect()` / `handleGapEvent()` | `NimBLE-Arduino 1.4.3 `NimBLEClient.cpp`` | Receives `BLE_GAP_EVENT_CONNECT` with `status=13`. |

---

## 7. Root-Cause Analysis

### 7.1 `status=13` still means `BLE_ERR_REM_USER_CONN_TERM`

Same as v0.1.8: `13` is the HCI/Core error `BLE_ERR_REM_USER_CONN_TERM` ("Remote User Terminated Connection"). The peer or the peer’s controller is actively rejecting the connection request or tearing down the link immediately.

### 7.2 Leading hypothesis: connect address is byte-reversed

The JOIN stores the peer address from the scan callback using `advertisedDevice->getAddress().getNative()`:

```cpp
uint8_t addr[6];
memcpy(addr, advertisedDevice->getAddress().getNative(), 6);
```

`NimBLEAddress::getNative()` returns the internal NimBLE representation, which is the **inverse** of the canonical display order. When the JOIN later reconstructs a `NimBLEAddress` for `connect()`:

```cpp
NimBLEAddress addr(addr_buf, peer_addr_type_);
cli->connect(addr);
```

the `NimBLEAddress(uint8_t[6], type)` constructor reverse-copies the supplied bytes. Because the supplied bytes were already in NimBLE internal order, the resulting internal address is in canonical display order, and the controller transmits the reversed address over the air. In other words, the JOIN is trying to connect to `92:8d:a9:f4:e9:d4` instead of the actual HOST advertisement address `d4:e9:f4:a9:8d:92`.

This explains why:

- `NimBLEScan` reports the HOST as `d4:e9:f4:a9:8d:92`.
- The JOIN logs `connecting to 92:8d:a9:f4:e9:d4`.
- The HOST receives a connection request to a different address and rejects it with `status=13`.
- The failure is identical on every retry (it is not a race).

### 7.3 Secondary hypothesis: HOST advertising restart rotates the random address

`become_host()` stops and restarts advertising:

```cpp
stop_scanning();
stop_advertising();
vTaskDelay(pdMS_TO_TICKS(20));
start_advertising();
```

If NimBLE regenerates the device’s random advertisement address on this restart, the JOIN’s captured address becomes stale. The JOIN does not scan after role resolution, so it would never learn the new address. Even if the byte-order bug above is fixed, a stale address would still produce `status=13`.

### 7.4 Tertiary hypotheses (less likely given the clean role resolution)

- **Address-type mismatch:** The log shows `type=0` for the discovered peer. If the HOST is actually advertising with a random address type but NimBLE reports it as public, the controller may reject the connect request. This is unlikely because both sides see the same type.
- **Stale bonding:** v0.1.9 calls `NimBLEDevice::deleteAllBonds()` during `init()`, so persistent security data should not be a factor.
- **Local controller race:** The 50 ms guard delay should have removed the stop-advertise → connect race. The deterministic retry pattern suggests this is not the primary cause.

---

## 8. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.cpp` | `RpsAdvertisedDeviceCallbacks::onResult()` copies `getNative()` bytes; `connect_to_host()` reconstructs `NimBLEAddress` from those bytes, double-reversing the address. `become_host()` restarts advertising and may rotate the random address. |
| `src/state_machine/ble_service.h` | No functional bug, but the `status=13` comment was corrected in v0.1.9. |

---

## 9. Recommended Fix Directions

1. **Fix the peer-address byte order for `connect()`.**
   - Option A: store the canonical string representation (`advertisedDevice->getAddress().toString()`) and construct the connect address from that string.
   - Option B: reverse the stored native bytes before passing them to the `NimBLEAddress` constructor in `connect_to_host()`.
   - Verify by logging: the connect attempt address must match the address reported by `NimBLEScan`.

2. **Prevent HOST address rotation after role resolution.**
   - If the HOST restart in `become_host()` causes NimBLE to generate a new random address, either:
     - do not restart advertising (only stop scanning and let the existing advertisement continue), or
     - set a static random address / public address before advertising so the address does not change.

3. **Consider connecting to the peer’s public MAC instead of the random advertisement address.**
   - The public MAC is already exchanged in the manufacturer data and is stable.
   - If the HOST can be made to advertise and accept connections on its public MAC, the JOIN can connect to the public MAC with `BLE_ADDR_PUBLIC`, eliminating both the byte-order and address-rotation risks.

4. **Add explicit connect-address assertion.**
   - Before `cli->connect(addr)`, assert that `addr.toString()` matches the original scan-reported address. This would have caught the byte-order bug immediately.

---

## 10. Validation Needed After Fix

1. Reflash both CYD2USB v3 boards with the corrected firmware.
2. Reset both units and capture dual-serial logs.
3. Confirm:
   - `JOIN: advertising stopped, connecting to <addr>` address matches `NimBLEScan: New advertiser: <addr>`.
   - `NimBLEClient` logs `Connection established` instead of `status=13`.
   - Both units post `EV_CONNECTED` and reach multiplayer `SCR_Gameplay`.
4. Test timing variants:
   - COM5 Play first, then COM6.
   - COM6 Play first, then COM5.
   - Both Play as close to simultaneously as possible.
5. Confirm single-player fallback still works when only one unit is present.

---

## 11. Additional Reverse-Order Test

After the initial failure, both units were reset and the test was run in the opposite order:

- **COM5** (lower MAC / expected HOST) tapped **Play** first at `09:06:06.051`.
- **COM5** then tapped **Cancel** almost immediately.
- **COM6** (higher MAC / expected JOIN) tapped **Play** at `09:06:19.729`.
- COM6 discovered the HOST, resolved to JOIN, and entered the connect window.
- COM6 later started a second negotiation at `09:07:11.267` after a Retry/Play sequence.

In both attempts the failure pattern was identical:

```text
[09:06:19.731] COM6> I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
[09:06:19.731] COM6> BLE: on_peer_found addr=92:8d:a9:f4:e9:d4 type=0 public_valid=1
[09:06:19.731] COM6> BLE: resolve_role local=d4:e9:f4:af:42:a2 peer=92:8d:a9:f4:e9:d4 cmp=16 role=JOIN
...
[09:06:21.774] COM6> JOIN: advertising stopped, connecting to 92:8d:a9:f4:e9:d4
[09:06:26.796] COM6> E NimBLEClient: Connection failed; status=13
```

Key takeaways from the reverse-order run:

1. **The failure is independent of which unit starts first.** Both COM6-first and COM5-first orderings result in the same `status=13` sequence.
2. **The byte-order mismatch is consistent.** Every connect attempt logs `connecting to 92:8d:a9:f4:e9:d4` while the scan reports the peer as `d4:e9:f4:a9:8d:92`.
3. **The HOST does resolve and restart advertising cleanly.** COM5 logs `HOST: stopped scan, advertising ready` in the first-order run; the restart itself is not preventing the connection.
4. **UI Cancel touches do not affect the radio task connect sequence.** In the second attempt, COM6 received many Cancel events while the radio task was retrying `connect()`; the retries continued and failed identically, confirming the failure is in the radio-layer connect path, not in UI state handling.
5. **The 50 ms guard delay after `stop_advertising()` is not sufficient to change the outcome.** Each connect attempt still fails with `status=13`.

These observations strengthen the byte-order / address-construction hypothesis and make a pure timing-race explanation unlikely.

## 12. Post-Report Notes

- The v0.1.9 guard delay and HOST advertising restart were correctly implemented but did not resolve the underlying `status=13` failure.
- The instrumentation added in v0.1.9 was essential: without the `connecting to ...` log, the byte-order mismatch would not be visible.
- This report supersedes the v0.1.8 hypothesis that the failure is primarily a stop-advertising → connect race. The race may still exist, but the deterministic byte-order problem is now the leading explanation.
