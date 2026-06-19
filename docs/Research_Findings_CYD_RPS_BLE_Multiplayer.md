# Research Findings: CYD_RPS BLE Multiplayer Connection Failures

**Project:** `projects/CYD_RPS`  
**Firmware versions reviewed:** v0.1.0 → v0.1.6  
**Board:** ESP32-2432S028R CYD2USB v3  
**Library stack:** NimBLE-Arduino 1.4.3 (`h2zero/NimBLE-Arduino@^1.4.0`)  
**Date:** 2026-06-19  

---

## 1. Scope and Method

This document combines:

1. A review of every CYD_RPS bug report in `projects/CYD_RPS/docs/`.
2. A direct inspection of the current BLE implementation (`src/state_machine/ble_service.cpp`, `ble_service.h`, `app_state_machine.cpp`) and the installed NimBLE-Arduino 1.4.3 source.
3. Web research on NimBLE GAP behavior, ESP32 concurrent BLE roles, HCI error codes, and example patterns used by other ESP32/NimBLE projects.

No source code was changed. The goal is to identify the most likely cause of the v0.1.6 two-board failure and to point to robust design patterns for the next fix.

---

## 2. Bug History Summary

| Version | Symptom | Root cause identified | Fix applied |
|---------|---------|----------------------|-------------|
| v0.1.0 | Peer-search never times out to single-player | `platformio.ini` default env was the Wokwi build; physical hardware flashed a binary that skips `BleService::init()`, so `start_discovery()` returned silently and the timeout guard never fired. | Default env changed to physical hardware; `start_discovery()` now posts `EV_ERROR` when BLE is not initialized. |
| v0.1.1 | Device resets when **Play** is tapped | `BleService::start_discovery()` was called synchronously from the LVGL/state-machine dispatch path and started scan + advertise back-to-back without checking return values. | Discovery deferred to the next `update()` tick; dedicated radio task created. |
| v0.1.2 / v0.1.3 | Reset with `E NimBLEServer: ble_gatts_start; rc=15` / `abort()` | `NimBLEAdvertising::start()` lazily started the GATT server while a GAP scan was already active; `ble_gatts_start()` returns `BLE_HS_EBUSY` and the library calls `abort()`. | GATT server explicitly started in `BleService::init()` before any GAP procedure. |
| v0.1.4 | JOIN fails with `E NimBLEClient: Connection failed; status=13` | `connect_to_host()` rebuilt the peer `NimBLEAddress` with a hard-coded `BLE_ADDR_PUBLIC`, but the peer advertised a random address. | `peer_addr_type_` captured from the advertisement and used in `NimBLEAddress(addr_buf, peer_addr_type_)`. |
| v0.1.5 | Asymmetric role negotiation; JOIN stuck on "Connecting", HOST falls back to single-player | `become_join()` called `stop_advertising()`, so the peer that should become HOST never discovered the JOIN and eventually timed out. | (Intended fix in v0.1.6) Keep JOIN advertising during negotiation; add public-MAC exchange; retry connect. |
| v0.1.6 | Serial-connected unit discovers peer, becomes HOST, then sits on "Connecting" forever; no `EV_CONNECTED` | **Under investigation** — see Sections 5 and 6. | — |

---

## 3. Technical Observations from the Current Source

### 3.1 Role-resolution logic

`BleService::resolve_role()` (`src/state_machine/ble_service.cpp:350-378`) compares the local public MAC with the peer's public MAC extracted from advertisement manufacturer data:

```cpp
const int cmp = memcmp(local, peer_id, 6);
if (cmp < 0) {
    become_host();
} else {
    become_join(peer_addr_);
}
```

- `local` comes from `NimBLEDevice::getAddress().getNative()`.
- `peer_id` is `peer_public_addr_` when `peer_public_addr_valid_` is true; otherwise it falls back to the random advertisement address `peer_addr_`.

The serial log from Unit B in the v0.1.6 test (`d4:e9:f4:a9:8d:90`) shows:

```text
I NimBLEScan: New advertiser: d4:e9:f4:af:42:a2
I NimBLEScan: Updated advertiser: d4:e9:f4:af:42:a2
BLE: start_advertising heap=169776
```

The extra `BLE: start_advertising` after the peer is found is consistent with Unit B resolving as **HOST** and executing `become_host()` → `start_advertising()`. Because Unit B has the lower public MAC (`d4:e9:f4:a9:8d:90` < `d4:e9:f4:af:42:a0`), HOST is the correct role **if** manufacturer data was parsed on both sides.

What is unknown is the state of Unit A. If Unit A also resolved as HOST (for example because it failed to parse the manufacturer data and used random-address fallback), no central would ever initiate the link.

### 3.2 Manufacturer-data encoding and parsing

The v0.1.6 payload is:

```cpp
std::vector<uint8_t> manu;
manu.push_back(static_cast<uint8_t>(kManufacturerCompanyId & 0xFF));   // 0xFF
manu.push_back(static_cast<uint8_t>((kManufacturerCompanyId >> 8) & 0xFF)); // 0xFF
const uint8_t* local = NimBLEDevice::getAddress().getNative();
manu.insert(manu.end(), local, local + 6);  // public MAC
adv->setManufacturerData(manu);
```

Relevant NimBLE-Arduino 1.4.3 facts:

- `NimBLEAdvertising::setManufacturerData(const std::vector<uint8_t>&)` exists and copies the bytes verbatim (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEAdvertising.cpp:170-175`).
- `NimBLEAdvertisedDevice::getManufacturerData()` searches the combined advertisement payload for `BLE_HS_ADV_TYPE_MFG_DATA` and returns the field value as a `std::string` (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEAdvertisedDevice.cpp:165-177`).
- With active scanning on legacy advertisements, `NimBLEScan` deliberately **waits for the scan response** before invoking `onResult` for `ADV_IND`/`ADV_SCAN_IND` devices (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEScan.cpp:136-154`). By the time the callback runs, the stored payload should contain both the advertisement and the scan response.
- The current CYD_RPS advertising payload is exactly 31 bytes: 3 bytes flags + 18 bytes for the 128-bit service UUID + 8 bytes manufacturer data + 2-byte length/type overhead. This fills the legacy advertising packet; the default scan response is enabled but empty because no device name is set.

A subtle risk: `BleService::on_peer_found()` returns immediately if `peer_addr_valid_` is already true (`ble_service.cpp:318-319`). Because `NimBLEScan` suppresses duplicate callbacks via `m_callbackSent`, this early return is normally harmless, but if the first callback ever fires with incomplete manufacturer data (e.g. a scan response is lost and the device is reported at scan-end), the parser will never get a second chance.

### 3.3 Address types and the ESP32 MAC

- `NimBLEDevice::m_own_addr_type` defaults to `BLE_OWN_ADDR_PUBLIC` (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEDevice.cpp:87`).
- `NimBLEDevice::getAddress()` returns the public address if one exists, otherwise the current random address (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEDevice.cpp:442-452`).
- The ESP32 factory **BLE public address is the Wi-Fi/base MAC + 2** (confirmed by [espressif/arduino-esp32#8609](https://github.com/espressif/arduino-esp32/issues/8609)). This explains why the scanned address `d4:e9:f4:af:42:a2` differs by two from the base MAC `d4:e9:f4:af:42:a0` listed in the environment notes: the scanned address **is** the BLE public address.
- With public-address advertising, the captured `peer_addr_type_` should be `BLE_ADDR_PUBLIC`. The v0.1.4 fix remains a good safety net, but it is not the active failure mode in v0.1.6.

### 3.4 The connection path

`BleService::connect_to_host()` (`ble_service.cpp:414-487`):

1. Creates a fresh `NimBLEClient`.
2. Rebuilds `NimBLEAddress` from `peer_addr_` and `peer_addr_type_`.
3. Retries `cli->connect(addr)` up to 4 times with a 250 ms `vTaskDelay` backoff.

`NimBLEClient::connect()` in 1.4.3:

- Default timeout is **30 seconds** (`m_connectTimeout = 30000` ms).
- Internally loops on `BLE_HS_EBUSY` to stop the local scan, then blocks the calling task until the connection completes, fails, or times out (`.pio/libdeps/.../NimBLE-Arduino/src/NimBLEClient.cpp:188-317`).
- The 250 ms backoff in CYD_RPS is swallowed if a single connect attempt hangs for 30 s.

`ble_gap_connect()` in the NimBLE host rejects the call if local discovery is active, but it does **not** reject the call if local advertising is active (`.pio/libdeps/.../NimBLE-Arduino/src/nimble/nimble/host/src/ble_gap.c:5364-5372`). The controller may still reject the resulting link with `status=13`.

The v0.1.6 `become_join()` deliberately keeps advertising:

```cpp
stop_scanning();
// Do NOT call stop_advertising() here.
```

Then the state machine immediately transitions to "Connecting" and queues `CONNECT_TO_HOST`. The JOIN therefore attempts to act as a **central connecting to a peer** while still acting as a **peripheral that is advertising**.

---

## 4. Web Research Findings

### 4.1 Concurrent central + peripheral on ESP32

NimBLE advertises "Support for all 4 roles concurrently — Broadcaster, Observer, Peripheral and Central" ([h2zero/esp-nimble-component](https://github.com/h2zero/esp-nimble-component)). The ESP32 controller, however, has practical limits. An ESP32 forum post on peripheral + central simultaneously states:

> "For example it is highly advised to stop scan when you are trying to connect to peripheral and advertising should be stopped by low level API ..."  
> — [esp32.com/viewtopic.php?t=23158](https://esp32.com/viewtopic.php?t=23158), 8 Sept 2021

This matches the v0.1.6 code path: the JOIN stops scanning but **does not stop advertising** before initiating the connection.

### 4.2 `status=13` meaning

The `E NimBLEClient: Connection failed; status=13` log maps to the HCI error:

```text
0x020d  0x0d  BLE_ERR_CONN_REJ_RESOURCES  Connection Rejected due to Limited Resources
```

Source: [Apache Mynewt NimBLE return-code reference](https://mynewt.apache.org/latest/network/ble_hs/ble_hs_return_codes.html).

In CYD_RPS history this code appeared:

- In v0.1.4 because the JOIN used the wrong peer address type.
- In v0.1.5 because the HOST was not in a connectable state when the JOIN tried to connect.

It can also be returned when the local controller cannot accept the new master role because it is already busy with another GAP procedure such as advertising.

### 4.3 Example patterns

Standard NimBLE central sketches follow this pattern:

1. Start scanning.
2. In `onResult()`, **stop scanning**.
3. Create a client and **connect**.
4. Do **not** advertise while connecting.

The Arduino Forum "Arduino and BLE on ESP32 as server and client combined, using NimBLE" example ([forum.arduino.cc/t/.../1151247](https://forum.arduino.cc/t/arduino-and-ble-on-esp32-as-server-and-client-combined-using-nimble/1151247)) keeps the device as both server and client, but it still stops the scan before connecting and does not keep advertising during the connect phase.

---

## 5. Most Likely Root Cause of the v0.1.6 Failure

The leading hypothesis is a **controller-level resource conflict on the JOIN device** caused by advertising while initiating a connection:

1. Both units start discovery (scan + advertise).
2. Unit A discovers Unit B and resolves as JOIN.
3. Unit A's `become_join()` stops scanning but keeps advertising so Unit B can still discover it.
4. The state machine immediately moves to "Connecting" and queues `CONNECT_TO_HOST`.
5. Unit A now tries to act as a **central** while still acting as a **peripheral advertiser**. The ESP32 controller rejects or hangs the connect attempt.
6. Because `NimBLEClient::connect()` defaults to a 30 s timeout and CYD_RPS only retries 4 times with 250 ms backoffs, the JOIN can remain blocked for tens of seconds. The HOST (Unit B) sees no incoming connection and also stays on "Connecting".

The serial log from Unit B is consistent with this: Unit B became HOST and is waiting for a central that never connects.

### Secondary hypotheses

| # | Hypothesis | Evidence for | Evidence against |
|---|------------|--------------|------------------|
| A | Manufacturer-data parsing failed on Unit A, so both units resolved as HOST. | Explains why no side connects; the fallback to random-address ordering is not guaranteed to be symmetric. | Unit B's log shows it resolved as HOST correctly, and the manufacturer-data parser has the correct data available by the time `onResult` is called. |
| B | Unit A tries to connect before Unit B has finished resolving to HOST. | v0.1.5 was caused by a similar race; the 30 s connect timeout can absorb this. | Unit B's log shows it already called `start_advertising` after resolving, so it should be ready. |
| C | The 30 s connect timeout and 250 ms retry backoff make the JOIN appear "stuck" even though it will eventually error out. | `connect()` blocks for `m_connectTimeout + 1000` ms per attempt; 4 attempts can take > 2 minutes. | Would eventually produce an error/retry screen on Unit A, not observed because Unit A was not monitored. |

The strongest explanation is **the primary hypothesis (advertising while connecting)** combined with **the overly long connect timeout / short retry count**.

---

## 6. Diagnostic Gaps

The v0.1.6 test only captured serial output from one unit (Unit B). To confirm the root cause, the next test should capture both sides simultaneously and log:

- Local public MAC and the parsed peer public MAC.
- Whether `peer_public_addr_valid_` is true.
- Resolved role (`HOST`/`JOIN`).
- Each `connect()` attempt start/end and its result.
- Whether `stop_advertising()` is called before `connect()`.

Without Unit A's log we cannot distinguish the primary hypothesis from Hypothesis A (asymmetric manufacturer-data parse).

---

## 7. Recommended Design Changes for the Next Fix

These are research-based recommendations only; no code changes were made.

1. **Stop JOIN advertising before connecting, after a grace period.**
   - Keep advertising for ~500–1000 ms after role resolution so the HOST has time to discover the JOIN and resolve its own role.
   - Then call `stop_advertising()` and only then call `connect_to_host()`.
   - This directly addresses the ESP32 controller limitation noted in Section 4.1.

2. **Shorten the connect timeout and use a sensible retry/backoff strategy.**
   - `NimBLEClient::setConnectTimeout(uint8_t seconds)` can reduce the per-attempt timeout from 30 s to, e.g., 5 s.
   - Increase the retry count and use a 500–1000 ms backoff so the JOIN can wait for the HOST without blocking the radio task for minutes.

3. **Add explicit serial diagnostics.**
   - Print local public MAC, peer public MAC from manufacturer data, random advertisement address, resolved role, and each connect attempt.
   - This makes future two-board tests unambiguous.

4. **Capture logs from both units.**
   - A single-unit log cannot distinguish "JOIN cannot connect" from "both sides are HOST".

5. **Consider using `NimBLEClient::connect(NimBLEAdvertisedDevice*)`.**
   - The 1.4.3 library provides this overload. It preserves the address type automatically and removes the need to reconstruct `NimBLEAddress` manually.

6. **Alternative simpler architecture (longer-term).**
   - Because ESP32 advertises its public BLE address by default, the project could force public-address advertising and resolve roles directly from the scanned address, eliminating the manufacturer-data parsing path and the random-address fallback.
   - If a random address must be retained, consider adding a small "ready" flag in the manufacturer data so the JOIN can confirm the HOST is in connectable state before stopping its own advertising.

---

## 8. Empirical Confirmation (two-board test, 2026-06-19)

After the research document was first written, both units were connected simultaneously to USB serial and a fresh v0.1.6 test was run.

### Test setup

- COM5 and COM6: USB-SERIAL CH340 (CYD2USB v3 boards).
- Both units flashed with CYD_RPS v0.1.6.
- Dual serial logger captured both ports at 115200 baud.
- Unit on COM6 tapped **Play** first; unit on COM5 tapped **Play** ~1 second later.

### Observed sequence

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

- **COM6 became HOST** (lower public MAC, logged the post-resolution `BLE: start_advertising`).
- **COM5 became JOIN** (higher public MAC, no post-resolution advertising log).
- Role resolution worked symmetrically using the manufacturer-data public MACs.

### Connection failure

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

Key facts from this capture:

1. **Role resolution is correct.** Both units agreed on HOST (COM6) and JOIN (COM5).
2. **The failure is not an address-type mismatch.** The JOIN captured the peer as a public address and reconstructed the `NimBLEAddress` correctly.
3. **The failure is `status=13` on every connect attempt.** This is `BLE_ERR_CONN_REJ_RESOURCES`, consistent with a controller-level resource conflict.
4. **Each attempt took exactly 30 seconds.** This matches `NimBLEClient::connect()`’s default 30-second timeout. The 250 ms retry backoff in CYD_RPS is irrelevant when each attempt blocks for the full timeout.
5. **The HOST continued advertising and never saw an incoming connection.** The JOIN never got a link established.

### Updated conclusion

The two-board test strongly supports the primary hypothesis in Section 5: the JOIN attempts to connect while still advertising, and the ESP32 controller rejects each connection attempt with `status=13`. The 30-second default connect timeout makes the failure appear as a long “Connecting” delay before the error screen appears.

The secondary hypotheses (asymmetric manufacturer-data parsing, HOST not ready, wrong address type) are now considered unlikely because:

- Both units resolved roles consistently.
- The HOST was advertising before the first JOIN connect attempt began.
- The captured peer address type was public, matching the address used by the HOST.

## 9. References

### Local source files

- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.1.md`
- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.2.md`
- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.4.md`
- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.5.md`
- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.6.md`
- `projects/CYD_RPS/src/state_machine/ble_service.h`
- `projects/CYD_RPS/src/state_machine/ble_service.cpp`
- `projects/CYD_RPS/src/state_machine/app_state_machine.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/library.properties` (version 1.4.3)
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEAdvertising.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEAdvertisedDevice.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEClient.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEDevice.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEScan.cpp`
- `projects/CYD_RPS/.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/nimble/nimble/host/src/ble_gap.c`

### Web references

- [NimBLE-Arduino issue #646 — setManufacturerData with std::string null-byte problem](https://github.com/h2zero/NimBLE-Arduino/issues/646)
- [NimBLE-Arduino issue #708 — `Connection failed; status=13` with random address](https://github.com/h2zero/NimBLE-Arduino/issues/708)
- [NimBLE-Arduino issue #662 — connection failures status=574 / 0x3e](https://github.com/h2zero/NimBLE-Arduino/issues/662)
- [NimBLE-Arduino class reference — `NimBLEDevice::setOwnAddrType`](https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_device.html)
- [NimBLE-Arduino class reference — `NimBLEClient::connect`](https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_client.html)
- [Apache Mynewt — NimBLE host return codes](https://mynewt.apache.org/latest/network/ble_hs/ble_hs_return_codes.html)
- [ESP32 forum — BLE peripheral and central simultaneously](https://esp32.com/viewtopic.php?t=23158)
- [Arduino Forum — Arduino and BLE on ESP32 as server and client combined, using NimBLE](https://forum.arduino.cc/t/arduino-and-ble-on-esp32-as-server-and-client-combined-using-nimble/1151247)
- [Last Minute Engineers — ESP32 NimBLE tutorial](https://lastminuteengineers.com/esp32-nimble-arduino-tutorial/)
- [espressif/arduino-esp32#8609 — BLE MAC is WiFi MAC + 2](https://github.com/espressif/arduino-esp32/issues/8609)
- [h2zero/esp-nimble-component — NimBLE role/features description](https://github.com/h2zero/esp-nimble-component)
