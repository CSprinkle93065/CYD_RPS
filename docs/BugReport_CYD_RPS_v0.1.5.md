# Bug Report: CYD_RPS v0.1.5 — Multiplayer connection still fails; JOIN/HOST role negotiation is asymmetric

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.5 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.4.md` (status=13 due to missing peer address type) |
| **Severity** | High — two-player mode does not work on physical hardware |
| **Status** | **Fixed in v0.1.6** |
| **Reported** | 2026-06-18 |

---

## 1. Summary

v0.1.5 added capture of the peer BLE address type from scan advertisements and uses that type when the JOIN role calls `NimBLEClient::connect()`. The `Connection failed; status=13` error still occurs during two-board multiplayer, but the failure is now part of an **asymmetric role-negotiation sequence**:

- The unit that starts first and discovers the peer becomes **JOIN** (central), displays **"Connecting"**, and attempts to connect.
- The unit that starts second becomes **HOST** (peripheral) but continues to show **"searching..."**.
- After the discovery timeout, the second unit falls back to **standalone gameplay**.
- The first unit then shows a **connection error**.

Both units discover the peer’s random advertisement address, but the JOIN stops advertising when it resolves its role, so the HOST never observes an active peer while it is still scanning, and the JOIN’s connection attempt to the HOST’s advertisement fails before the HOST has finished role resolution.

---

## 2. Reproduction Steps

1. Flash `projects/CYD_RPS` v0.1.5 to two CYD2USB v3 boards.
2. Reset both boards.
3. Tap **Play** on the serial-connected unit (Unit A, MAC `d4:e9:f4:af:42:a0`).
   - Screen shows `SCR_PeerSearch` / **"searching..."**.
4. Within a few seconds, tap **Play** on the second unit (Unit B, MAC `d4:e9:f4:a9:8d:90`).
   - Unit A discovers Unit B and transitions to **"Connecting"**.
   - Unit B remains on **"searching..."**.
5. Wait for the discovery timeout (~10 s).
   - Unit B transitions to standalone gameplay.
   - Unit A shows a connection error / retry screen.

---

## 3. Expected Behavior

Per `docs/definition.md` and the v0.1.5 fix intent:

1. Both units enter `SCR_PeerSearch` and perform concurrent scan + advertise.
2. Both units discover each other’s advertisement.
3. Role resolution uses the public MAC ordering: lower MAC becomes HOST, higher MAC becomes JOIN.
4. The HOST stops scanning and continues advertising.
5. The JOIN stops scanning, keeps advertising long enough for the HOST to confirm the peer, and initiates the connection.
6. After GATT discovery and subscription, both boards reach multiplayer `SCR_Gameplay`.

---

## 4. Observed Behavior

Serial log captured from Unit A (COM5, 115200 baud):

```text
ets Jul 29 2019 12:21:46
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
CYD_RPS setup start
I NimBLEDevice: BLE Host Task Started
I NimBLEDevice: NimBle host synced.
CYD_RPS setup done
TOUCH raw=(2209,1937) screen=(137,153) z=1811
TOUCH raw=(2217,1938) screen=(137,153) z=1863
TOUCH raw=(2236,1932) screen=(139,153) z=1904
TOUCH raw=(2228,1934) screen=(138,153) z=1886
UI: Play button clicked
BLE: start_discovery heap=173228
BLE: start_advertising_if_needed heap=173152
BLE: start_advertising heap=173152
I NimBLEScan: New advertiser: 4f:12:fd:f9:04:70
I NimBLEScan: Updated advertiser: 4f:12:fd:f9:04:70
I NimBLEScan: New advertiser: 06:57:28:15:06:f4
I NimBLEScan: New advertiser: d8:34:1c:16:21:a7
...
I NimBLEScan: New advertiser: d4:e9:f4:a9:8d:92
I NimBLEScan: Updated advertiser: d4:e9:f4:a9:8d:92
E NimBLEClient: Connection failed; status=13
```

Unit A (serial) | Unit B (other)
---|---
Tap **Play** | —
`SCR_PeerSearch` / "searching..." | —
— | Tap **Play**
Discovers Unit B | Still "searching..."
`SCR_PeerSearch` / "Connecting" | Still "searching..."
Still "Connecting" | Falls back to standalone gameplay
Connection error / Retry | —

No `EV_CONNECTED` is posted on either unit.

---

## 5. Decoded Failure Location

The failure spans the role-resolution and connection paths in the firmware logic layer:

| Function | File | Role |
|----------|------|------|
| `app::BleService::connect_to_host()` | `src/state_machine/ble_service.cpp` | JOIN central attempts the BLE link. |
| `app::BleService::become_join()` | `src/state_machine/ble_service.cpp` | JOIN stops scan/advertise and prepares to connect. |
| `app::BleService::become_host()` | `src/state_machine/ble_service.cpp` | HOST stops scanning and ensures advertising is active. |
| `app::BleService::resolve_role()` | `src/state_machine/ble_service.cpp` | Compares local/peer addresses and selects HOST/JOIN. |
| `app::BleService::on_peer_found()` | `src/state_machine/ble_service.cpp` | Stores the first matching peer address and address type. |
| `app::BleService::radio_task_loop()` | `src/state_machine/ble_service.cpp` | Dispatches `BECOME_HOST` / `CONNECT_TO_HOST` commands. |

---

## 6. Root-Cause Analysis

### 6.1 The JOIN stops advertising before the peer has resolved its role

When Unit A discovers Unit B it immediately posts `EV_PEER_FOUND`. The state machine calls `signal_become_host()`, which queues `BECOME_HOST` on the radio task. `resolve_role()` determines Unit A has the higher MAC and calls `become_join()`. `become_join()` executes:

```cpp
stop_advertising();
stop_scanning();
```

At this point Unit B may still be in `SCR_PeerSearch` and has not yet discovered Unit A. Because Unit A has stopped advertising, Unit B never sees a peer advertisement, so it never posts `EV_PEER_FOUND`, never resolves to HOST, and eventually times out to standalone gameplay.

### 6.2 The JOIN attempts to connect before the HOST is ready

Immediately after `become_join()`, the state machine transitions to `Connecting` and queues `CONNECT_TO_HOST`. `connect_to_host()` uses the random advertisement address captured during discovery:

```cpp
NimBLEAddress addr(addr_buf, peer_addr_type_);
if (!cli->connect(addr)) { ... }
```

Because Unit B has not yet resolved to HOST (and may still be scanning, or may have timed out and stopped advertising), the connection request fails with `status=13`.

### 6.3 Role resolution relies on a random advertisement address

`resolve_role()` compares the local public MAC (`NimBLEDevice::getAddress()`) against the stored `peer_addr_`, which is the random address captured from the peer’s advertisement. The random address is not guaranteed to have the same ordering as the public MACs. If both boards discover each other, the ordering could be inconsistent, causing both units to select the same role or the wrong role.

For reliable role resolution, both peers need a **common, stable identifier** (e.g., the public MAC exchanged through the advertisement payload, or a deterministic hash agreed by both sides).

### 6.4 `status=13` persists even with the correct address type

v0.1.5 fixed the address-type mismatch, but the connection still receives `status=13`. This indicates that the peer is not in a connectable state when the request is issued, consistent with the HOST not having finished role resolution or having already given up because it never discovered the JOIN.

---

## 7. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.h` | `peer_addr_type_` field; `on_peer_found()` signature. |
| `src/state_machine/ble_service.cpp:337-363` | `become_join()` stops advertising; `connect_to_host()` timing. |
| `src/state_machine/ble_service.cpp:316-335` | `resolve_role()` compares public MAC to random advertisement address. |
| `src/state_machine/ble_service.cpp:352-418` | JOIN connection path. |
| `docs/definition.md` | May need to specify the role-resolution algorithm and required advertisement payload. |

---

## 8. Recommended Fix Directions

### 8.1 Do not stop advertising on the JOIN side during role negotiation

In `become_join()`, stop scanning but **keep advertising** so the HOST can still discover the JOIN and resolve its own role:

```cpp
void BleService::become_join(const uint8_t peer_addr[6]) {
    role_ = Role::JOIN;
    advertising_pending_ = false;
    stop_scanning();
    // Keep advertising so the peer can discover us and resolve its HOST role.
    // Do NOT call stop_advertising() here.
    memcpy(peer_addr_, peer_addr, 6);
    peer_addr_valid_ = true;
    sm_post_event(Event::EV_ROLE_RESOLVED);
}
```

### 8.2 Add a connection retry or wait-for-host ready state

The JOIN should not give up on the first `status=13`. Options:

- Retry `NimBLEClient::connect()` with a short backoff for a bounded number of attempts.
- Introduce a `WaitForHost` sub-state that verifies the HOST is still advertising before attempting the connection.
- Wait for the connection to succeed within a timeout before posting `EV_ERROR`.

### 8.3 Exchange public MACs in the advertisement payload

Embed the public MAC (`NimBLEDevice::getAddress()`) in the manufacturer-specific data of the advertisement. The scanner can then use the peer’s **public** MAC for role resolution instead of the random advertisement address. This guarantees deterministic, symmetric role assignment.

If the payload cannot be expanded, use the random address for connection but resolve roles by comparing the public MACs carried in the scan response/advertisement data.

### 8.4 Ensure the HOST stops scanning and continues advertising cleanly

`become_host()` already calls `stop_scanning()` and `start_advertising()`. Verify that `start_advertising()` does **not** restart advertising with a new random address, because that would invalidate the address the JOIN captured. If NimBLE generates a new random address, either:

- Set a static random address once during `init()` and reuse it, or
- Have the JOIN re-scan for the HOST’s current address before connecting.

---

## 9. Secondary Observations

- Free heap during discovery (~173 KB) is healthy; this is not a memory issue.
- The v0.1.4 `BLE_HS_EBUSY` abort and the v0.1.5 address-type capture remain in place and are not implicated in this failure.
- Touch input and display rendering continue to work correctly on both units.

---

## 10. Fix Applied in v0.1.6

The v0.1.6 firmware-logic revision addresses the three root causes above:

1. **JOIN keeps advertising during role negotiation.** `BleService::become_join()` now calls `stop_scanning()` but deliberately does **not** call `stop_advertising()`, so the peer that becomes HOST can still discover the JOIN and resolve its own role.

2. **JOIN retries the connection.** `BleService::connect_to_host()` now attempts `NimBLEClient::connect()` up to `kConnectRetries` (4) times with a 250 ms backoff between attempts. Only after all retries fail does it post `EV_ERROR`.

3. **Role resolution uses the public MAC.** `BleService::start_advertising()` embeds the local public MAC (`NimBLEDevice::getAddress()`) in the BLE advertisement manufacturer-specific data (company ID `0xFFFF` + 6-byte public MAC). `RpsAdvertisedDeviceCallbacks::onResult()` extracts the peer public MAC from the manufacturer data and stores it in `peer_public_addr_`. `BleService::resolve_role()` compares local/peer public MACs when available; otherwise it falls back to the random advertisement address. The random address is still used for `NimBLEClient::connect()`.

4. **HOST avoids regenerating its random address.** `BleService::become_host()` stops scanning and calls `start_advertising()`, which returns immediately without restarting if advertising is already active.

All NimBLE radio work continues to run on the dedicated `ble_radio` task. The v0.1.4 and v0.1.5 fixes (address-type capture, GATT server early start, event-queue mutex, `WOKWI_SIMULATION` skip, etc.) are preserved.

### Files Changed

- `src/state_machine/ble_service.h`
- `src/state_machine/ble_service.cpp`
- `docs/BugReport_CYD_RPS_v0.1.5.md`

### Fix Status

| Field | Value |
|-------|-------|
| **Status** | **Fixed in v0.1.6** |
| **Fixed by** | Firmware Logic Coding Agent (Stage 11 bug-fix revision) |
| **Validation** | `pio run -e esp32-2432s028r_cyd2usb` compiles; `pio run -e esp32-2432s028r_cyd2usb_wokwi` compiles; two-board physical validation pending. |

---

## 11. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB v3 boards.
2. Start both units with various timing offsets (simultaneous, A first, B first).
3. Confirm both units discover each other and resolve roles symmetrically.
4. Confirm the JOIN unit connects successfully (`EV_CONNECTED` posted, no `status=13`).
5. Confirm both units reach multiplayer `SCR_Gameplay` and exchange moves.
6. Confirm the fallback to single-player still works when only one unit is present.
