# Bug Report: CYD_RPS v0.1.8 — Multiplayer still fails; JOIN discovers HOST but `connect()` is rejected with `status=13`

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.8 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.7.md` (JOIN stopped advertising before HOST could discover it) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | Fixed in v0.1.9 |
| **Reported** | 2026-06-20 |

---

## 1. Summary

v0.1.8 attempted to fix the v0.1.7 deadlock by:

- Keeping the JOIN advertising for a bounded `kJoinDiscoveryWindowMs = 2000 ms` window before the first `connect()` attempt.
- Restarting advertising for `kJoinConnectInterRetryWindowMs = 2000 ms` between each failed `connect()` retry.
- Shortening the peer-search advertising interval to 20–30 ms.

After flashing v0.1.8 to two CYD2USB v3 boards, the JOIN side **does discover the HOST**, and the HOST appears to resolve its own role (it stops scanning and continues advertising). However, every JOIN→HOST `connect()` attempt still fails with `status=13`. The failure mode has shifted from *“HOST never discovered JOIN”* to *“HOST discovered JOIN but refused/terminated the connection”*. The prior bug report misidentified `status=13` as a local timeout; this report re-examines the NimBLE source and shows that `13` is the HCI error code `BLE_ERR_REM_USER_CONN_TERM` — i.e. the peer actively rejected or tore down the link.

---

## 2. Environment

- **Board:** ESP32-2432S028R CYD2USB v3
- **Firmware:** CYD_RPS v0.1.8 (GitHub release `CYD_RPS_v0.1.8.zip` + `firmware.bin`)
- **NimBLE-Arduino:** 1.4.3
- **Units:**
  - Unit A (COM5): public MAC `d4:e9:f4:af:42:a0` — higher MAC, expected JOIN
  - Unit B (COM6): public MAC `d4:e9:f4:a9:8d:90` — lower MAC, expected HOST
- **Serial:** COM5 and COM6 at 115200 baud, captured simultaneously with `.tmp/dual_serial_logger.py`
- **Test setup:** Both units reset, COM6 tapped **Play** first, then COM5 tapped **Play** a few seconds later.

---

## 3. Reproduction Steps

1. Flash v0.1.8 to both boards.
2. Reset both boards.
3. Tap **Play** on COM6 (lower MAC / expected HOST).
4. Tap **Play** on COM5 (higher MAC / expected JOIN) within a few seconds.
5. Observe both screens and the dual-serial log.

---

## 4. Expected Behavior

1. Both units enter `SCR_PeerSearch` and perform concurrent scan + advertise.
2. Each unit discovers the other’s RPS-service advertisement.
3. Role resolution uses the public MAC ordering: lower MAC (`d4:e9:f4:a9:8d:90`) becomes HOST, higher MAC (`d4:e9:f4:af:42:a0`) becomes JOIN.
4. The HOST stops scanning and continues advertising connectably.
5. The JOIN keeps advertising long enough for the HOST to discover it, then stops advertising and initiates the connection.
6. The link is established and both units reach multiplayer `SCR_Gameplay`.

---

## 5. Observed Behavior

Dual-serial log (`.tmp/cyd_rps_dual_serial_log.txt`, timestamps UTC):

```text
[07:51:24.124] COM6> UI: Play button clicked
[07:51:24.124] COM6> BLE: start_discovery heap=173220
[07:51:24.124] COM6> BLE: start_advertising_if_needed heap=173148
[07:51:24.124] COM6> BLE: start_advertising heap=173148
...
[07:51:25.629] COM5> UI: Play button clicked
[07:51:25.629] COM5> BLE: start_discovery heap=173220
[07:51:25.630] COM5> BLE: start_advertising_if_needed heap=173144
[07:51:25.630] COM5> BLE: start_advertising heap=173144
[07:51:25.631] COM5> I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
[07:51:25.632] COM5> BLE: initial discovery window before connect
[07:51:25.633] COM5> BLE: start_advertising heap=171200
...
[07:51:25.685] COM6> I NimBLEScan: New advertiser: d4:e9:f4:af:42:a2
[07:51:25.686] COM6> BLE: start_advertising heap=169264
...
[07:51:32.645] COM5> E NimBLEClient: Connection failed; status=13
[07:51:32.645] COM5> BLE: connect attempt 1/4 failed
[07:51:32.646] COM5> BLE: connect retry 2/4 - discovery window
[07:51:32.646] COM5> BLE: start_advertising heap=171200
[07:51:39.679] COM5> E NimBLEClient: Connection failed; status=13
[07:51:39.680] COM5> BLE: connect attempt 2/4 failed
[07:51:46.688] COM5> E NimBLEClient: Connection failed; status=13
[07:51:46.689] COM5> BLE: connect attempt 3/4 failed
[07:51:53.712] COM5> E NimBLEClient: Connection failed; status=13
[07:51:53.713] COM5> BLE: connect attempt 4/4 failed
[07:51:53.713] COM5> ERROR: BLE join connection failed after retries
```

Key observations:

- **COM5 (expected JOIN) discovered COM6 at 07:51:25.631** and immediately entered the `connect_to_host()` discovery window.
- **COM6 (expected HOST) discovered COM5 at 07:51:25.685** and, 1 ms later, logged `BLE: start_advertising heap=169264`. That timing and heap change are consistent with COM6 resolving to HOST inside `BleService::become_host()` (which calls `stop_scanning()` and then `start_advertising()`).
- **COM6 never logs another application message** for the rest of the capture, so we cannot tell whether it accepted, rejected, or even saw the connection requests.
- **All four JOIN→HOST connect attempts failed with `status=13`** at roughly 7-second intervals (2 s discovery window + 5 s `NimBLEClient` connect timeout).
- After the last retry, COM5 restarted advertising and the state machine eventually posted `EV_ERROR`.

---

## 6. Decoded Failure Location

| Function | File | Role in this failure |
|----------|------|----------------------|
| `RpsAdvertisedDeviceCallbacks::onResult()` | `src/state_machine/ble_service.cpp` | Filters RPS advertisements and extracts the peer public MAC. Appears to have fired on both sides. |
| `BleService::on_peer_found()` | `src/state_machine/ble_service.cpp` | Stores peer address/address-type and posts `EV_PEER_FOUND`. |
| `BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Compares local public MAC with peer public MAC. COM6 lower → `become_host()`; COM5 higher → `become_join()`. |
| `BleService::become_host()` | `src/state_machine/ble_service.cpp` | Stops scanning and continues advertising. The COM6 log line at 07:51:25.686 strongly suggests this path ran. |
| `BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | JOIN stops advertising and calls `NimBLEClient::connect()`; all four attempts returned `false` with `status=13`. |
| `NimBLEClient::connect()` / `handleGapEvent()` | `NimBLE-Arduino 1.4.3 `NimBLEClient.cpp`` | Receives the `BLE_GAP_EVENT_CONNECT` event with `status=13` and logs it. |

---

## 7. Root-Cause Analysis

### 7.1 `status=13` is `BLE_ERR_REM_USER_CONN_TERM`, not a local timeout

`NimBLEUtils::returnCodeToString()` in NimBLE-Arduino 1.4.3 maps `0x0200 + BLE_ERR_REM_USER_CONN_TERM` to the string **“Remote User Terminated Connection”**:

```cpp
// NimBLE-Arduino 1.4.3 / src/NimBLEUtils.cpp
        case  (0x0200+BLE_ERR_REM_USER_CONN_TERM ):
            return "Remote User Terminated Connection";
```

The underlying HCI/Core error enum defines it as `0x13`:

```cpp
// NimBLE-Arduino 1.4.3 / src/nimble/nimble/include/nimble/ble.h
    BLE_ERR_REM_USER_CONN_TERM  = 0x13,
```

The earlier reports (`docs/BugReport_CYD_RPS_v0.1.7.md` §11.1, and the `ble_service.h` comment block) describe `status=13` as a NimBLE host timeout (`BLE_HS_ETIMEOUT`). That was imprecise. `BLE_HS_ETIMEOUT` is a host-level return code (≈ `0x205`) and would produce the message **“Operation timed out.”** The value `13` is an HCI status returned by the controller, and in this context it means the *remote* side terminated the connection request or the newly formed connection.

Practical implication: the JOIN is no longer timing out because the peer is invisible; it is reaching the peer, and the peer (or the peer’s controller) is actively saying no.

### 7.2 The HOST almost certainly resolved its role this time

In the v0.1.7 failure the intended HOST never logged a discovery of the JOIN. In this v0.1.8 log:

- COM6 sees `New advertiser: d4:e9:f4:af:42:a2` at 07:51:25.685.
- At 07:51:25.686 COM6 logs `BLE: start_advertising heap=169264`.
- The only places `start_advertising()` is invoked from the radio task after discovery are `become_host()` and `become_join()`. COM6 has the lower public MAC, so `resolve_role()` must call `become_host()`.
- `become_host()` first calls `stop_scanning()` and then `start_advertising()`. The heap drop (~3.9 KB) is consistent with scan-state teardown and the radio-task call tree.

Therefore the most plausible reading is that **COM6 successfully resolved to HOST, stopped scanning, and remained advertising** by 07:51:25.686. The JOIN’s first `connect()` did not start until ~07:51:27.6 (after the 2 s discovery window), so the HOST had roughly 2 seconds to settle into peripheral-only mode.

### 7.3 The failure is now in the JOIN→HOST connection establishment phase

Because the discovery/role-negotiation problem from v0.1.7 appears solved, the remaining `status=13` must come from one of the following mechanisms:

#### 7.3.1 Race between stopping JOIN advertising and initiating the connection

`connect_to_host()` does:

```cpp
stop_advertising();
if (cli->connect(addr)) { ... }
```

There is **no delay** between `stop_advertising()` and `connect()`. `NimBLEAdvertising::stop()` simply calls `ble_gap_adv_stop()` and returns; it does not wait for the controller to fully leave the advertiser state. The ESP32 NimBLE/Bluedroid controller is known to reject a central `connect` attempt while the local device is still acting as a peripheral advertiser (this is why v0.1.6/v0.1.7 added `stop_advertising()` in the first place). If the advertiser teardown is not complete when `ble_gap_connect()` is issued, the controller may report the failure as a remote-terminated connection event with status `0x13`.

This is especially likely because each of the four retries repeats the exact same sequence:

1. `start_advertising()`
2. `vTaskDelay(kJoinConnectInterRetryWindowMs)`
3. `stop_advertising()`
4. immediate `connect()`

If the bug is in the advertising-stop → connect-start transition, every retry will fail the same way, exactly as observed.

#### 7.3.2 HOST never fully restarted connectable advertising after stopping scan

`become_host()` does:

```cpp
stop_scanning();
if (!start_advertising()) { ... }
```

`start_advertising()` contains:

```cpp
if (adv->isAdvertising()) {
    return true;
}
return adv->start();
```

Because the HOST was already advertising from the initial discovery phase, `become_host()` returns immediately without re-issuing `ble_gap_adv_start()`. The advertisement started while the device was **both scanning and advertising**; the ESP32 controller may not cleanly transition that combined role into a peripheral-only acceptor without an explicit advertising restart. If the controller considers the ongoing advertising context “stale” or non-connectable after scan teardown, it could reject incoming connection requests.

#### 7.3.3 Captured random address is stale or address-type mismatch

Both units are advertising with random-style addresses (e.g. COM6 appears as `d4:e9:f4:a9:8d:92` in scans, not its public MAC `d4:e9:f4:a9:8d:90`). The JOIN stores `peer_addr_type_` from `advertisedDevice->getAddress().getType()` and later constructs `NimBLEAddress(addr_buf, peer_addr_type_)`. If the HOST rotates its resolvable private address (RPA) between the moment the JOIN captured it and the moment the JOIN connects, the captured address is stale and the HOST controller will reject the request. The log shows no new advertiser address for COM6, but RPAs can rotate due to host-based privacy timers or an advertising restart on the HOST side.

#### 7.3.4 Stale bonding/identity data in NVS

`NimBLEDevice::init()` calls `ble_store_config_init()`, which reads persistent security keys from NVS. Earlier firmware versions (v0.1.4–v0.1.7) ran pairing/bonding-disabled code, but NimBLE may still have stored peer identity information from previous connection attempts. If the HOST has a stale IRK or bonding record for the JOIN’s public MAC, it could reject an unauthenticated incoming connection with reason `0x13`. This is less likely but should be ruled out before declaring the radio-transition theory proven.

#### 7.3.5 Connection parameters rejected by the HOST controller

The JOIN uses NimBLEClient default connection parameters:

```cpp
m_pConnParams.scan_itvl = 16;
m_pConnParams.scan_window = 16;
m_pConnParams.itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN; // 24 * 1.25 ms
m_pConnParams.itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX; // 40 * 1.25 ms
m_pConnParams.latency  = 0;
m_pConnParams.supervision_timeout = BLE_GAP_INITIAL_SUPERVISION_TIMEOUT; // 400 * 10 ms
```

A controller rejecting parameters would normally return `0x3B` (Unacceptable Connection Parameters) or `0x3E` (Connection Failed to be Established), not `0x13`. So this is unlikely to be the primary cause.

### 7.4 Leading hypothesis

The most probable root cause is a **state-transition race on one or both sides**:

1. The JOIN stops advertising and immediately calls `connect()` before the ESP32 controller has fully left the advertiser role.
2. The HOST stops scanning but relies on an advertising context that was started while the device was also scanning; that context may not be accepted by the controller as a valid peripheral target.

Either condition would cause the peer/controller to reject the connection with `BLE_ERR_REM_USER_CONN_TERM (0x13)`, and both conditions are present on every retry, explaining the four identical failures.

### 7.5 Why v0.1.8’s discovery-window fix did not remove the failure

v0.1.8 fixed the *discovery* half of the handshake (the HOST now sees the JOIN), but it did not address the *connection-establishment* half. The retry logic keeps giving the HOST fresh discovery windows, but because the same stop-advertise→connect race occurs every time, the retry loop is deterministic in its failure.

---

## 8. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.h` | `kJoinDiscoveryWindowMs`, `kJoinConnectInterRetryWindowMs`, `kPeerSearchAdvMin/MaxInterval`; the header comment still incorrectly labels `status=13` as a timeout and should be corrected in the next fix. |
| `src/state_machine/ble_service.cpp` | `connect_to_host()` stops advertising and immediately connects; `become_host()` does not restart advertising after stopping scan. |
| `src/state_machine/app_state_machine.cpp` | State transitions into `Connecting` and the radio-task commands are correct, but the actions provide no instrumentation to confirm HOST role resolution. |
| `docs/state_machine.puml` | Already has `RoleNegotiating` and `Connecting` states; the handshake logic is sound at the PUML level. |

---

## 9. Recommended Fix Directions

1. **Add instrumentation before changing code.** Insert `Serial.println` logs in:
   - `BleService::on_peer_found()` — log captured address, address type, and public-MAC validity.
   - `BleService::resolve_role()` — log local vs peer MAC comparison and chosen role.
   - `BleService::become_host()` — log “HOST: stopped scan, advertising ready”.
   - `BleService::become_join()` — log “JOIN: host discovered, starting connect window”.
   - `BleService::connect_to_host()` — log “JOIN: advertising stopped, connecting to <addr>”.
   This will confirm whether the HOST is actually resolving and whether it accepts/rejects each connection attempt.

2. **Insert a guard delay after `stop_advertising()` and before `connect()`.** In `connect_to_host()`:
   ```cpp
   stop_advertising();
   // Wait for the controller to leave the advertiser state.
   vTaskDelay(pdMS_TO_TICKS(50));
   if (cli->connect(addr)) { ... }
   ```
   A 50–100 ms delay is small compared with the 5 s connect timeout and removes the advertising-teardown race. Alternatively, poll `adv->isAdvertising()` until it returns false with a short timeout.

3. **Restart advertising explicitly in `become_host()`.** Instead of relying on the advertisement that was started while scanning, `become_host()` should stop and restart advertising after `stop_scanning()` so the HOST enters a clean peripheral-only connectable state:
   ```cpp
   stop_scanning();
   stop_advertising();
   vTaskDelay(pdMS_TO_TICKS(20));
   if (!start_advertising()) { ... }
   ```
   Care must be taken not to regenerate the random advertisement address, because the JOIN has already captured it. If `adv->isAdvertising()` is false, `start_advertising()` will call `adv->start()`; if the address changes, the JOIN’s stored address becomes stale. Consider using `NimBLEDevice::setOwnAddrType()` or a fixed static random address if RPAs are rotating.

4. **Consider directed advertising from HOST to JOIN once roles are resolved.** After the HOST knows the JOIN’s public MAC and random address, it could switch to directed connectable advertising toward the JOIN. That avoids the risk that the JOIN connects to a stale RPA and guarantees that the JOIN is the only device that can initiate.

5. **Clear or ignore NimBLE bonding data.** Add `NimBLEDevice::deleteAllBonds()` during `BleService::init()` (or at the start of each discovery cycle) to eliminate stale security state as a variable. If long-term bonding is ever desired, it can be added back intentionally later.

6. **Update the misleading comment in `ble_service.h`.** Replace the `BLE_HS_ETIMEOUT` / “Operation timed out” description with the correct `BLE_ERR_REM_USER_CONN_TERM` mapping.

---

## 10. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB v3 boards.
2. Reset both units and capture dual-serial logs for these timing variants:
   - COM6 Play first, then COM5.
   - COM5 Play first, then COM6.
   - Both Play as close to simultaneously as possible.
3. Confirm in the log:
   - The intended HOST logs `become_host()` / “HOST: advertising ready”.
   - The intended JOIN logs `become_join()` and, after stopping advertising, “JOIN: connecting to …”.
   - `NimBLEClient` logs `Connection established` instead of `Connection failed; status=13`.
   - Both units post `EV_CONNECTED` and reach multiplayer `SCR_Gameplay`.
4. Confirm single-player fallback still works when only one unit is present.
5. Confirm resetting bonding data does not break reconnection if the feature is later reintroduced.

---

## 11. Post-Report Research Notes

### 11.1 NimBLE connect path

`NimBLEClient::connect()` sets `m_connectTimeout` from `setConnectTimeout()` and issues `ble_gap_connect()`:

```cpp
// NimBLEClient.cpp
rc = ble_gap_connect(NimBLEDevice::m_own_addr_type, &peerAddr_t,
                     m_connectTimeout, &m_pConnParams,
                     NimBLEClient::handleGapEvent, this);
```

When the controller reports a connect event with non-zero status, the client logs:

```cpp
NIMBLE_LOGE(LOG_TAG, "Connection failed; status=%d %s",
            taskData.rc, NimBLEUtils::returnCodeToString(taskData.rc));
```

The decimal value `13` therefore comes directly from the controller’s connect-complete event, not from the host’s timeout machinery.

### 11.2 `ble_gap_rx_conn_complete` failure handling

`NimBLE-Arduino/src/nimble/nimble/host/src/ble_gap.c` handles a failed `LE Connection Complete` event only for specific status codes (`BLE_ERR_DIR_ADV_TMO`, `BLE_ERR_UNK_CONN_ID`). An `0x13` status is not handled there, confirming it is delivered to the client callback as a real remote status rather than a host-local timeout.

### 11.3 Advertising restart behavior

`NimBLEAdvertising::start()` returns early if `ble_gap_adv_active()` is true:

```cpp
if(ble_gap_adv_active()) {
    NIMBLE_LOGW(LOG_TAG, "Advertising already active");
    return true;
}
```

Consequently, `become_host()` does **not** reconfigure advertising after stopping scan. If the existing advertisement was started under a combined scan+advertise controller state, it may not be a valid target for an incoming connection once scanning stops.

### 11.4 Advertising stop is not synchronous

`NimBLEAdvertising::stop()` only calls `ble_gap_adv_stop()` and returns:

```cpp
int rc = ble_gap_adv_stop();
if (rc != 0 && rc != BLE_HS_EALREADY) { ... }
return true;
```

There is no wait for `ble_gap_adv_active()` to become false. Calling `connect()` immediately after `stop()` risks attempting a central role while the local controller is still a peripheral advertiser.

### 11.5 Why the 7-second retry cadence persists

Each retry in `connect_to_host()` is:

- 2000 ms advertising window (`kJoinConnectInterRetryWindowMs`), then
- 5000 ms `NimBLEClient` connect timeout (`kConnectTimeoutSeconds`).

That matches the observed ~7-second spacing between `status=13` messages. The interval is a consequence of the constants, not the root cause.

### 11.6 References

- `NimBLEClient.cpp` release/1.4: `connect()`, `setConnectTimeout()`, `handleGapEvent()`.
- `NimBLEUtils.cpp` release/1.4: `returnCodeToString()` mapping for `BLE_ERR_REM_USER_CONN_TERM`.
- `NimBLEAdvertising.cpp` release/1.4: `start()`, `stop()`, `isAdvertising()`.
- `ble_gap.c` NimBLE host: `ble_gap_rx_conn_complete()`, `ble_gap_accept_slave_conn()`.
- `nimble/include/nimble/ble.h`: HCI error code definitions, `BLE_ERR_REM_USER_CONN_TERM = 0x13`.
- Prior project report: `docs/BugReport_CYD_RPS_v0.1.7.md`.
