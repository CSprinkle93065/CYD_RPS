# Bug Report: CYD_RPS v0.1.4 — Multiplayer connection fails with `NimBLEClient: Connection failed; status=13`

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.4 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.2.md` (Play-reset due to BLE_HS_EBUSY) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | **Fixed in v0.1.5** |
| **Reported** | 2026-06-18 |

---

## 1. Summary

After the v0.1.4 fix for the Play-reset crash, the firmware now boots to `SCR_Start`, starts discovery, and finds the peer on both CYD2USB v3 units. However, the unit that resolves to the **JOIN** (central) role cannot connect to the peer. It reports `E NimBLEClient: Connection failed; status=13` and transitions to the error screen. Pressing **Retry** falls back to standalone gameplay; two boards never establish a BLE link and cannot play a multiplayer round.

The v0.1.3/v0.1.4 work (dedicated radio task, GATT-server start during init, heap guard) fixed the earlier reset path but exposed this connection-address-type issue once discovery succeeded.

---

## 2. Reproduction Steps

1. Flash `projects/CYD_RPS` v0.1.4 to two CYD2USB v3 boards:
   ```bash
   python execution/flash_cyd2usb.py --project projects/CYD_RPS
   ```
2. Power both boards.
3. Tap **Play** on both boards within a few seconds of each other.
4. Wait for role negotiation.
5. Observe that one board becomes **HOST** (peripheral) and the other becomes **JOIN** (central).
6. The JOIN board fails to connect and shows an error/retry screen.

---

## 3. Expected Behavior

Per `docs/definition.md`, after peer discovery the boards should:

1. Resolve roles (lower public MAC = HOST, higher public MAC = JOIN).
2. The HOST stops scanning and starts advertising.
3. The JOIN stops scanning and initiates a connection to the HOST.
4. After GATT service/characteristic discovery and subscription, both boards reach multiplayer `SCR_Gameplay`.

---

## 4. Observed Behavior

Serial log captured from the JOIN unit (COM5, 115200 baud):

```text
CYD_RPS setup start
I NimBLEDevice: BLE Host Task Started
I NimBLEDevice: NimBle host synced.
CYD_RPS setup done
UI: Play button clicked
BLE: start_discovery heap=173228
BLE: start_advertising_if_needed heap=173152
BLE: start_advertising heap=173152
I NimBLEScan: New advertiser: 4d:41:d5:92:9d:28
I NimBLEScan: New advertiser: 68:95:75:40:d7:c2
I NimBLEScan: New advertiser: d8:34:1c:16:21:a7
...
I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
I NimBLEScan: Updated advertiser: d4:e9:f4:a9:8d:92
E NimBLEClient: Connection failed; status=13
UI: Retry button clicked
E NimBLEServer: ble_gap_terminate failed: rc=7
```

After the error, the JOIN board offers a retry. Tapping retry restarts discovery but the same failure repeats. The HOST board continues advertising but is never connected.

---

## 5. Decoded Failure Location

The connection failure originates in the JOIN-side connection path:

| Function | File | Role |
|----------|------|------|
| `app::BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | Creates `NimBLEClient`, connects to stored peer address. |
| `app::BleService::become_join()` | `src/state_machine/ble_service.cpp` | Stops scan/advertise, posts `EV_ROLE_RESOLVED`. |
| `app::BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Compares local/peer MACs and calls `become_host()` or `become_join()`. |
| `app::BleService::on_peer_found()` | `src/state_machine/ble_service.cpp` | Stores the first matching peer address. |

`connect_to_host()` constructs the peer address with a hard-coded public type:

```cpp
// src/state_machine/ble_service.cpp
uint8_t addr_buf[6];
memcpy(addr_buf, peer_addr_, 6);
NimBLEAddress addr(addr_buf, BLE_ADDR_PUBLIC);   // <-- type is assumed
if (!cli->connect(addr)) {
    sm_post_event(Event::EV_ERROR);
    return;
}
```

`status=13` is reported by `NimBLEClient` in the `ble_gap_connect` completion event. In NimBLE this is the raw HCI status code `0x0D`, which corresponds to an address/connection-establishment rejection (`BLE_ERR_CONN_LIMIT` / `BLE_ERR_CONN_REJ_RESOURCES` depending on NimBLE version). The common observed cause in NimBLE when the address bytes are correct but the connection still fails with status 13 is an **incorrect peer address type**.

---

## 6. Root-Cause Analysis

### 6.1 Address bytes are captured, address type is not

`BleService::on_peer_found()` receives a `NimBLEAdvertisedDevice` from the scan callback. It currently stores only the raw 6-byte address:

```cpp
void BleService::on_peer_found(const uint8_t peer_addr[6]) {
    ...
    memcpy(peer_addr_, peer_addr, 6);
    peer_addr_valid_ = true;
    sm_post_event(Event::EV_PEER_FOUND);
}
```

`NimBLEAdvertisedDevice::getAddress()` returns a `NimBLEAddress` that also carries the **address type** (`BLE_ADDR_PUBLIC`, `BLE_ADDR_RANDOM`, `BLE_ADDR_PUBLIC_ID`, or `BLE_ADDR_RANDOM_ID`). That type is lost.

### 6.2 JOIN connection assumes `BLE_ADDR_PUBLIC`

`become_join()` reconstructs the peer address with `BLE_ADDR_PUBLIC`:

```cpp
NimBLEAddress addr(addr_buf, BLE_ADDR_PUBLIC);
```

If the HOST board advertised using a random or resolvable-random address, the JOIN will send a connection request with the right bytes but the wrong type. The controller rejects it with status 13.

### 6.3 Why one board always fails

Role resolution is deterministic: the board with the lower public MAC becomes HOST, the higher one becomes JOIN. Whichever board is JOIN always runs the `connect_to_host()` path with the hard-coded public address type, so it always fails.

### 6.4 Why the HOST side does not show an error

The HOST stops scanning, starts advertising, and waits for a central to connect. It never sees an incoming connection because the JOIN's connect request uses the wrong address type and is rejected by the controller before the link is established.

---

## 7. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.h` | `BleService` stores `peer_addr_` but no `peer_addr_type_`. |
| `src/state_machine/ble_service.cpp:283-307` | `on_peer_found()` captures address bytes only. |
| `src/state_machine/ble_service.cpp:337-348` | `become_join()` assumes `BLE_ADDR_PUBLIC` when reconstructing the peer address. |
| `src/state_machine/ble_service.cpp:350-400` | `connect_to_host()` calls `cli->connect(addr)` with the reconstructed address. |

---

## 8. Recommended Fix

Capture the address type in `on_peer_found()` and use it when connecting.

### 8.1 Add a type field

In `src/state_machine/ble_service.h`:

```cpp
uint8_t peer_addr_[6] = {0};
uint8_t peer_addr_type_ = 0;  // BLE_ADDR_PUBLIC by default
bool peer_addr_valid_ = false;
```

### 8.2 Capture the type from the advertisement

In `src/state_machine/ble_service.cpp` `on_peer_found()`:

```cpp
memcpy(peer_addr_, peer_addr, 6);
peer_addr_valid_ = true;
peer_addr_type_ = advertisedDevice->getAddress().getType();
```

### 8.3 Use the captured type when connecting

In `src/state_machine/ble_service.cpp` `become_join()`:

```cpp
NimBLEAddress addr(addr_buf, peer_addr_type_);
if (!cli->connect(addr)) {
    sm_post_event(Event::EV_ERROR);
    return;
}
```

### 8.4 Reset the type on each new discovery cycle

In `do_start_discovery()`:

```cpp
peer_addr_type_ = 0;  // BLE_ADDR_PUBLIC default
```

---

## 9. Fix Applied in v0.1.5

The fix captures the BLE peer address type at discovery time and uses it when the JOIN role reconstructs the `NimBLEAddress` for `NimBLEClient::connect()`:

- `BleService` now stores `peer_addr_type_` alongside `peer_addr_`.
- `RpsAdvertisedDeviceCallbacks::onResult()` passes `advertisedDevice->getAddress().getType()` into `BleService::on_peer_found()`.
- `do_start_discovery()` and `do_stop_discovery()` reset `peer_addr_type_` to `0` (`BLE_ADDR_PUBLIC`) at the start of each discovery cycle so stale types do not carry across sessions.
- `connect_to_host()` constructs `NimBLEAddress(addr_buf, peer_addr_type_)` instead of hard-coding `BLE_ADDR_PUBLIC`.
- Inline comments cite `docs/BugReport_CYD_RPS_v0.1.4.md` for the address-type capture and usage.

These changes preserve all v0.1.4 fixes: GATT server start during `init()`, the dedicated radio task owning all NimBLE operations, the thread-safe event queue, non-blocking discovery, the `WOKWI_SIMULATION` guard, and the default physical-hardware PlatformIO environment.

## 10. Secondary Issue: `ble_gap_terminate failed: rc=7` on Retry

When the user presses **Retry**, the state machine calls `disconnect()` while there is no active connection. `NimBLEServer::disconnect()` returns `BLE_HS_ENOTCONN` (`rc=7`). This is harmless noise but should be cleaned up by only calling `disconnect()` when `connected_` is true or by suppressing the log when the disconnect is intentionally a no-op.

---

## 11. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB v3 boards.
2. Reset both boards and tap **Play** on each.
3. Confirm both boards discover each other and negotiate roles.
4. Confirm the JOIN unit connects successfully (`EV_CONNECTED` posted, no `status=13`).
5. Confirm both boards reach multiplayer `SCR_Gameplay` and can exchange moves.
6. Run several rounds to ensure the connection is stable across role swaps.

---

## 12. Workaround

There is no reliable workaround in v0.1.4. Single-player mode still works by letting the discovery timer expire.
