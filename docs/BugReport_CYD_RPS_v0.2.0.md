# Bug Report: CYD_RPS v0.2.0

**Reported:** 2026-06-21
**Project:** CYD_RPS
**Version:** 0.2.0
**Board:** ESP32-2432S028R CYD2USB v3
**Workflow ID:** wvc_20260617_171607

---

## Summary

Hardware verification of the v0.2.0 release revealed one Bluetooth peer-connection failure and five UI layout/rendering defects. These issues were observed on physical CYD2USB units flashed with the release build (`dist/CYD_RPS_v0.2.0.zip` / commit `f94eb37`).

---

## Issue B01 — Bluetooth auto-join does not connect

**Severity:** High
**Component:** `src/state_machine/ble_service.cpp`

### Observed behavior

- COM5 unit placed in Host mode via serial `HOST` command:
  ```
  SERIAL: HOST
  MODE: MULTI_PLAYER
  STATE: HostWait
  ```
- COM6 unit remained on the Start screen (scanning for Host advertisements).
- COM6's scan log repeatedly reported the COM5 BLE public MAC (`d4:e9:f4:a9:8d:92`).
- COM6 never transitioned to `Joining`, never printed `JOIN: connecting`, and never reached `Gameplay`.
- The Host wait timer eventually expired (15 s) and COM5 showed the timeout dialog.

### Expected behavior

When one unit is in `HostWait` and a second unit is on the Start screen, the second unit should detect the Host advertisement, transition to `Joining`, connect to the Host public MAC, and both units should enter `Gameplay`.

### Likely root cause

`RpsAdvertisedDeviceCallbacks::onResult()` filters incoming advertisements with:

```cpp
if (!advertisedDevice->isAdvertisingService(service_uuid)) {
    return;
}
```

The RPS service UUID is a 128-bit value. The Host advertisement also carries 12 bytes of manufacturer-specific data (company ID `0x00FF` + 4-char device ID + 6-byte public MAC). Combined with flags and address bytes, the 128-bit service UUID may not fit in the 31-byte BLE advertisement packet and may be sent in the scan response instead.

The scanner is created with `setDuplicateResults(false)` (default), so `onResult()` is invoked only once per discovered address. If the first invocation occurs before the scan response containing the service UUID is received, `isAdvertisingService()` returns false and the Host is ignored.

### Suggested fix direction

- Accept a Host advertisement when **either** the RPS service UUID is advertised **or** the manufacturer data matches the known presence-beacon signature (company ID `0x00FF`, 4-hex-digit device ID, 6-byte public MAC).
- Validate the manufacturer-data device ID is hex to avoid false joins on random `0xFF` beacons.
- Continue to extract the Host public MAC from the advertisement address if it is public, otherwise from the manufacturer data.

---

## Issue U01 — Scoreboard overlaps the "Choose!" label

**Severity:** Medium
**Component:** `src/ui/` gameplay screen

### Observed behavior

The scoreboard (`W:x L:x D:x`) is rendered on top of the "Choose!" label, making both hard to read.

### Expected behavior

Move the scoreboard to the **bottom center** of the gameplay screen.

### Impact on results screen

Moving the gameplay scoreboard down may affect the results screen layout. The "Play Again" button on the results screen must be moved **up** to avoid overlapping the relocated scoreboard.

---

## Issue U02 — Colored text rendering is poor

**Severity:** Medium
**Component:** `src/ui/` all screens

### Observed behavior

Colored text (non-white labels/buttons) appears low-quality on the white-on-black theme.

### Expected behavior

Render **all text in white** for now. The current accent color `#404040` can remain for backgrounds/borders, but text should be white.

---

## Issue U03 — Host-timeout popup buttons are too large and overlap

**Severity:** Medium
**Component:** `src/ui/` host timeout dialog

### Observed behavior

After the Host wait timer expires, the timeout dialog buttons are oversized and overlap each other.

### Expected behavior

Reduce the width of the timeout dialog buttons so they fit side-by-side (or stack cleanly) without overlapping.

---

## Issue U04 — Host timer bar color scheme is incorrect

**Severity:** Low
**Component:** `src/ui/` host wait screen

### Observed behavior

The Host wait progress bar is rendered in yellow / dark yellow.

### Expected behavior

Render the progress bar fill in **blue** with a **black** background.

---

## Issue U05 — Results screen scoreboard shows zero

**Severity:** Medium
**Component:** `src/ui/` results screen

### Observed behavior

After a round ends, the results screen scoreboard displays `W:0 L:0 D:0` even though the internal score is correct: when returning to the gameplay screen, the scoreboard shows the updated values.

### Expected behavior

The results screen scoreboard must display the same score values shown on the gameplay screen.

---

## Reproduction steps (general)

1. Flash two CYD2USB units with the v0.2.0 firmware.
2. Open serial monitors on both units (115200 baud).
3. Send `HOST\r\n` to unit A. Observe `STATE: HostWait`.
4. Leave unit B on the Start screen.
5. Observe that unit B never joins unit A (B01).
6. Play a solo round via the touch UI or serial `SOLO` / `ROCK` to observe gameplay/results UI issues (U01–U05).

---

## Attachments

- Build artifact: `.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`
- Release package: `dist/CYD_RPS_v0.2.0.zip`
- Source files involved:
  - `src/state_machine/ble_service.cpp`
  - `src/ui/` (gameplay, results, host wait, timeout dialog screens)
