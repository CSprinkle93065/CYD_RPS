# Bug Report: CYD_RPS v0.1.2 / v0.1.3 — Reset when Play is tapped while BLE scanning is starting

| Field | Value |
|-------|-------|
| **Project** | CYD_RPS |
| **Version** | 0.1.2 (observed); reproduced unchanged on 0.1.3 |
| **Previous bug** | `docs/BugReport_CYD_RPS_v0.1.1.md` (v0.1.1 reset before PeerSearch appeared) |
| **Severity** | High — one-tap start flow crashes on physical hardware |
| **Status** | **Fix implemented** in `src/state_machine/ble_service.cpp` — GATT server started during `BleService::init()` before any GAP procedure is active |
| **Reported** | 2026-06-18 |
| **Updated** | 2026-06-18 after v0.1.3 hardware test and serial capture |

---

## 1. Summary

After flashing the CYD_RPS physical-hardware build (`esp32-2432s028r_cyd2usb`) to a CYD2USB v3 board, the firmware boots to `SCR_Start`. Tapping **Play** immediately after boot reliably resets the device back to `SCR_Start`. The `SCR_PeerSearch` screen may appear for a fraction of a second, but the single-player fallback is never reached and two boards never discover each other.

The v0.1.3 fix (dedicated FreeRTOS radio task, larger NimBLE host stack, heap guard, reduced LVGL memory) changed *where* the crash occurs but did not remove it. Serial capture now shows that the crash is a deliberate `abort()` inside `NimBLEServer::start()` because `ble_gatts_start()` returns `BLE_HS_EBUSY` (`rc=15`).

---

## 2. Reproduction Steps

1. Build/flash `projects/CYD_RPS` with the default physical-hardware environment:
   ```bash
   python execution/flash_cyd2usb.py --project CYD_RPS
   ```
2. Reset or power-cycle the board.
3. As soon as `SCR_Start` appears, tap **Play**.
4. Observe the device reboot to `SCR_Start` within ~1 s.

---

## 3. Expected Behavior

Per `docs/definition.md` §2 and §3 (UA-02 / UA-06):

> *Start BLE discovery with a 10 s timeout and switch to `SCR_PeerSearch`.*  
> *If no peer is discovered, fall back to single-player `SCR_Gameplay`.*

`SCR_PeerSearch` should remain stable for the full discovery window, and two boards in range should discover each other and negotiate roles.

---

## 4. Observed Behavior

Serial log captured on COM5 (115200 baud):

```text
I NimBLEDevice: BLE Host Task Started
I NimBLEDevice: NimBle host synced.
CYD_RPS setup done
TOUCH raw=(2034,2146) screen=(125,173) z=1988
UI: Play button clicked
BLE: start_discovery heap=173572
BLE: start_advertising_if_needed heap=173500
BLE: start_advertising heap=173500
E NimBLEServer: ble_gatts_start; rc=15, 

abort() was called at PC 0x400da4ab on core 0

Backtrace: 0x40083889:0x3ffd9be0 0x4009385d:0x3ffd9c00 0x40098e11:0x3ffd9c20
0x400da4ab:0x3ffd9ca0 0x400d6afe:0x3ffd9d00 0x400d2423:0x3ffd9d60
0x400d262c:0x3ffd9da0 0x400d2725:0x3ffd9dc0 0x400d2759:0x3ffd9df0

Rebooting...
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

The pattern repeats on every boot if **Play** is tapped quickly. Free heap is ~173 KB, so this is **not** a heap-exhaustion crash.

---

## 5. Decoded Crash Location

Decoded with `xtensa-esp32-elf-addr2line` against `.pio/build/esp32-2432s028r_cyd2usb/firmware.elf`:

| Address | Function | File |
|---------|----------|------|
| `0x400da4ab` | `NimBLEServer::start()` | `NimBLE-Arduino/src/NimBLEServer.cpp:191` |
| `0x400d6afe` | `NimBLEAdvertising::start()` | `NimBLE-Arduino/src/NimBLEAdvertising.cpp:429` |
| `0x400d2423` | `app::BleService::start_advertising()` | `src/state_machine/ble_service.cpp:579` |
| `0x400d262c` | `app::BleService::start_advertising_if_needed()` | `src/state_machine/ble_service.cpp:536` |
| `0x400d2725` | `app::BleService::radio_task_loop()` | `src/state_machine/ble_service.cpp:480` |
| `0x400d2759` | `app::BleService::radio_task_entry(void*)` | `src/state_machine/ble_service.cpp:468` |

The crash is an unconditional `abort()` after `ble_gatts_start()` returns a non-zero code:

```cpp
// NimBLE-Arduino/src/NimBLEServer.cpp:187-192
int rc = ble_gatts_start();
if (rc != 0) {
    NIMBLE_LOGE(LOG_TAG, "ble_gatts_start; rc=%d, %s", rc, ...);
    abort();
}
```

---

## 6. Root-Cause Analysis

### 6.1 `rc=15` is `BLE_HS_EBUSY`

In NimBLE:

```cpp
// nimble/nimble/host/include/host/ble_hs.h
#define BLE_HS_EBUSY 15
```

The `ble_gatts_start()` implementation rejects the call when any GAP procedure is active:

```cpp
// nimble/nimble/host/src/ble_gatts.c
ble_hs_lock();
if (!ble_gatts_mutable()) {
    rc = BLE_HS_EBUSY;
    goto done;
}
```

`ble_gatts_mutable()` returns `false` if advertising, discovery (scanning), or a connection is active:

```cpp
if (ble_gap_adv_active() ||
    ble_gap_disc_active() ||
    ble_gap_conn_active()) {
    return false;
}
```

### 6.2 CYD_RPS starts scanning before advertising

`BleService::do_start_discovery()` (current v0.1.3 code):

```cpp
void BleService::do_start_discovery() {
    ...
    if (!start_scanning()) { ... }

    // Defer advertising startup by one cycle.
    advertising_pending_ = true;
    discovery_start_ms_ = millis();
}
```

In the radio-task path this returns straight into `start_advertising_if_needed()`:

```cpp
void BleService::radio_task_loop() {
    ...
    if (cmd == RadioCommand::START_DISCOVERY) {
        do_start_discovery();
        start_advertising_if_needed();   // called immediately
    }
}
```

`start_advertising_if_needed()` -> `start_advertising()` -> `NimBLEAdvertising::start()`. Because the GATT server has not been explicitly started yet, `NimBLEAdvertising::start()` calls `NimBLEServer::start()`:

```cpp
// NimBLE-Arduino/src/NimBLEAdvertising.cpp:426-434
NimBLEServer* pServer = NimBLEDevice::getServer();
if(pServer != nullptr) {
    if(!pServer->m_gattsStarted){
        pServer->start();
    }
}
```

At this point scanning is already active, so `ble_gatts_start()` returns `BLE_HS_EBUSY`, and the library calls `abort()`.

### 6.3 Why v0.1.3 did not fix it

The v0.1.3 changes were all necessary stack/heap-safety improvements, but none of them changed the order of GAP operations:

| v0.1.3 change | What it fixed | Why the reset still happens |
|---------------|---------------|-----------------------------|
| Dedicated `ble_radio` task (8192 words) | Moves NimBLE work off the Arduino loop/LVGL stack | The crash is inside `NimBLEServer::start()` on the radio-task stack, not a stack overflow |
| `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192` | Larger NimBLE host task stack | Not a stack-size abort |
| `MIN_HEAP_FOR_BLE_ADV = 8192` guard | Prevents advertising-data `realloc()` failure | Free heap is ~173 KB; guard does not fire |
| `LV_MEM_SIZE` 32 KiB | Leaves more heap for NimBLE | Heap is not the limiting factor |

The remaining defect is a ** sequencing/race condition **: the GATT server is started lazily while a GAP discovery procedure is already active.

### 6.4 Why two boards never pair

Both boards reset immediately after **Play**, so neither remains in `SCR_PeerSearch` long enough to scan or advertise. The BLE discovery/connection path is therefore never exercised end-to-end.

---

## 7. Affected Files

| File | Role in bug |
|------|-------------|
| `src/state_machine/ble_service.cpp:192-238` | `do_start_discovery()` starts scan before advertising. |
| `src/state_machine/ble_service.cpp:472-496` | `radio_task_loop()` calls `start_advertising_if_needed()` immediately after `do_start_discovery()`. |
| `src/state_machine/ble_service.cpp:559-580` | `start_advertising()` calls `adv->start()`, which triggers `NimBLEServer::start()`. |
| `src/state_machine/ble_service.cpp:86-120` | `BleService::init()` creates the server/service but does **not** call `NimBLEServer::start()`. |
| `.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEAdvertising.cpp:426-434` | Advertising start lazily starts the GATT server. |
| `.pio/libdeps/esp32-2432s028r_cyd2usb/NimBLE-Arduino/src/NimBLEServer.cpp:187-192` | `NimBLEServer::start()` calls `abort()` on any `ble_gatts_start()` error. |

---

## 8. Recommended Fixes

### 8.1 Preferred: start the GATT server during initialization

After creating the service and characteristic in `BleService::init()`, explicitly call `NimBLEServer::start()` while no GAP procedure is active. This sets `m_gattsStarted = true`, so later `NimBLEAdvertising::start()` will skip the server-start step and will not call `ble_gatts_start()` while scanning.

```cpp
bool BleService::init() {
    ...
    if (!service_ptr(service_)->start()) {
        return false;
    }

    // Start the GATT server now, before any scan/advertise GAP procedure
    // can make ble_gatts_start() return BLE_HS_EBUSY.
    server_ptr(server_)->start();

    initialized_ = true;
    return true;
}
```

Because `NimBLEDevice::init()` already waits for host-controller sync before returning, this call runs in a safe window.

### 8.2 Alternative: reverse the discovery order

In `do_start_discovery()` / `radio_task_loop()`, start advertising (and therefore the GATT server) **before** starting the scan. Once the server is started, scanning can be started safely. The user-visible behavior is unchanged.

### 8.3 Alternative: guard advertising startup against EBUSY

If the library cannot be avoided, `start_advertising()` could detect the busy condition and return `false` instead of letting the library abort. However, the abort is inside `NimBLEServer::start()`, which is outside CYD_RPS code, so the only reliable way is to prevent the EBUSY condition.

---

## 9. Workaround

Anecdotal: if the user waits approximately 15 seconds on `SCR_Start` before tapping **Play**, the board reportedly reaches `SCR_Gameplay` (single-player fallback) without resetting. This is **not reliable** and does not fix multiplayer discovery. It should not be used as a production workaround.

---

## 10. Validation Needed After Fix

1. Flash the corrected firmware to two CYD2USB boards.
2. Reset board A and tap **Play** immediately after `CYD_RPS setup done`.
3. Confirm `SCR_PeerSearch` remains stable for the full 10-second timeout and, if no peer is present, falls back to `SCR_Gameplay`.
4. Reset board B and tap **Play** while board A is still in `SCR_PeerSearch`.
5. Confirm the boards discover each other, negotiate roles (`ROLE: HOST` / `ROLE: JOIN`), and reach `SCR_Gameplay` in multiplayer mode.
6. Confirm a move can be selected on each board and the round evaluates to `SCR_Result`.
