# Bug Report: CYD_RPS v0.2.1

**Reported:** 2026-06-22
**Project:** CYD_RPS
**Version:** 0.2.1
**Board:** ESP32-2432S028R CYD2USB v3
**Serial ports:** COM5 (MAC `d4:e9:f4:a9:8d:90`), COM6 (MAC `d4:e9:f4:af:42:a0`)
**Test log:** `.tmp/CYD_RPS_command_test_20260622_001211.log`

---

## Summary

Release v0.2.1 fixed the BLE *discovery* failure described in the v0.2.0 report (B01), so a Join unit now detects the Host advertisement through the manufacturer-data beacon. However, hardware verification shows that the peer-to-peer connection still fails because **both boards resolve as Join**, the **`HOST` serial command cannot force a board into Host mode**, and the address used for `NimBLEClient::connect()` appears **byte-reversed**. In addition, the serial `HOME` and `RESET` commands both trigger a full CPU reset instead of a screen/state reset.

Single-player mode and serial move commands continue to work correctly.

---

## Issue B01-1 — BLE connection still fails with `status=13`

**Severity:** High  
**Component:** `src/state_machine/ble_service.cpp`, `src/state_machine/app_state_machine.cpp`

### Observed behavior

After v0.2.1, a Join unit detects the Host via the manufacturer-data beacon:

```text
[COM6] DISCOVERY: host seen via public addr d4:e9:f4:a9:8d:92 (svc=0 beacon=1)
[COM6] DISCOVERY: peer 92:8D:A9:F4:E9:D4 type=0
[COM6] ROLE: auto-join as JOIN
[COM6] MODE: MULTI_PLAYER
[COM6] STATE: Joining
[COM6] JOIN: connecting to 92:8d:a9:f4:e9:d4 (attempt 1/4)
[COM6] E NimBLEClient: Connection failed; status=13
[COM6] JOIN: connect attempt 1/4 failed
```

All four connection attempts fail with `status=13`:

```text
[COM6] JOIN: connecting to 92:8d:a9:f4:e9:d4 (attempt 1/4)
[COM6] JOIN: connecting to 92:8d:a9:f4:e9:d4 (attempt 2/4)
[COM6] JOIN: connecting to 92:8d:a9:f4:e9:d4 (attempt 3/4)
[COM6] JOIN: connecting to 92:8d:a9:f4:e9:d4 (attempt 4/4)
[COM6] ERROR: BLE join connection failed after retries
[COM6] STATE: Start
```

The failure is symmetric: whichever unit resolves as Join logs the same four failed attempts.

### Expected behavior

Once one unit is designated Host and the other is Join, the Join unit should connect to the Host public MAC and both units should enter `Gameplay`.

### Likely root causes

1. **Byte-reversed MAC address.**  
   COM5 public MAC is `d4:e9:f4:a9:8d:90`. The address COM6 attempts to connect to is `92:8d:a9:f4:e9:d4`, which is the exact reverse byte order. NimBLE `NimBLEAddress` may be constructed from a buffer whose byte order is not being handled consistently.

2. **No listener / both units resolve as Join.**  
   When both boards are left on the Start screen, each detects the other and transitions to `Joining`. No board stays in `HostWait`, so all connection attempts target a peer that is also scanning and not advertising as a connectable peripheral.

---

## Issue B01-2 — Symmetric role resolution: both boards become Join

**Severity:** High  
**Component:** `src/state_machine/app_state_machine.cpp`

### Observed behavior

With both boards on the Start screen, both independently detect each other and auto-resolve as Join:

```text
[COM5] DISCOVERY: host seen via public addr d4:e9:f4:af:42:a2 (svc=0 beacon=1)
[COM5] DISCOVERY: peer A2:42:AF:F4:E9:D4 type=0
[COM5] ROLE: auto-join as JOIN
[COM5] MODE: MULTI_PLAYER
[COM5] STATE: Joining
```

and simultaneously on COM6:

```text
[COM6] DISCOVERY: host seen via public addr d4:e9:f4:a9:8d:92 (svc=0 beacon=1)
[COM6] DISCOVERY: peer 92:8D:A9:F4:E9:D4 type=0
[COM6] ROLE: auto-join as JOIN
[COM6] MODE: MULTI_PLAYER
[COM6] STATE: Joining
```

Both then call `connect_to_host()` at roughly the same time. Lower public MAC does **not** reliably become Host.

### Expected behavior

Exactly one of the two scanning boards should become Host and the other should become Join, based on a stable comparison of public MAC addresses.

### Likely root causes

1. The conflict decision may be made before a stable Host/Join decision is reached, or the comparison may be using an address whose byte order is reversed, leading to inconsistent ordering.

2. There may be no hold-off / suppression window: as soon as a board sees *any* peer advertisement it immediately enters `Joining`, even if it should later decide to become Host.

---

## Issue B01-3 — `HOST` serial command is ignored while joining

**Severity:** High  
**Component:** `src/state_machine/app_state_machine.cpp` / serial command dispatch

### Observed behavior

Sending `HOST\r\n` to COM5 is echoed as `SERIAL: HOST`, but the state does not change:

```text
[COM5] SERIAL: HOST
[COM5] ...continues Joining...
[COM5] ERROR: BLE join connection failed after retries
[COM5] STATE: Start
```

The command never produces `STATE: HostWait` or `MODE: MULTI_PLAYER` in Host mode.

### Expected behavior

`HOST` should force the selected unit into `HostWait` and suppress its auto-join logic.

### Likely root causes

1. The serial command may be parsed only in certain states (e.g., `Start`), but by the time the command is received the board has already transitioned to `Joining`.

2. `become_host()` may be preempted by the active scan/connect cycle, or the event queue may drop the host request because the state machine is already processing a join transition.

---

## Issue U06 — `HOME` and `RESET` serial commands reboot the device

**Severity:** Medium  
**Component:** Serial command parser / reset handling

### Observed behavior

Every `HOME` or `RESET` command over serial is followed by:

```text
[COM5] SERIAL: HOME
[COM5] rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
[COM5] CYD_RPS setup start
[COM5] CYD_RPS setup done
[COM5] STATE: Start
```

and likewise for `RESET`:

```text
[COM5] SERIAL: RESET
[COM5] rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
[COM5] CYD_RPS setup start
[COM5] CYD_RPS setup done
[COM5] STATE: Start
```

### Expected behavior

- `HOME` should return the UI to the Start screen without rebooting.
- `RESET` should reset the stored score/state and return to the Start screen without rebooting.

### Likely root cause

The serial command handler may be calling `ESP.restart()` for both commands, or routing both to the same system-reset action.

---

## Verified working features

| Feature | Result | Evidence |
|---|---|---|
| Serial `SOLO` command | ✅ Works | `MODE: SINGLE_PLAYER`, `STATE: Start → Gameplay` |
| Serial move commands (`ROCK`, `PAPER`, `SCISSORS`) | ✅ Works | Gameplay → Evaluating → Result transitions |
| `AGAIN` command | ✅ Works | Returns to `Gameplay` and updates score |
| Score update logic | ✅ Works | Serial logs show `SCORE: W0 L1 D0`, etc. |
| Invalid command rejection | ✅ Works | `FOO` → `unknown command 'FOO'` |
| BLE manufacturer-data beacon detection | ✅ Works | `DISCOVERY: host seen via ... (svc=0 beacon=1)` |

> **Note:** The v0.2.1 UI fixes (U01–U05) cannot be verified from serial logs alone; they require visual inspection on the physical displays.

---

## Reproduction steps

1. Flash two CYD2USB units with `dist/CYD_RPS_v0.2.1.zip`.
2. Open serial monitors at 115200 baud on COM5 and COM6.
3. Leave both units on the Start screen.
4. Observe that both units auto-resolve as Join and log `ERROR: BLE join connection failed after retries` (B01-2, B01-1).
5. Try sending `HOST\r\n` to either unit after it returns to `Start`. Observe the command is echoed but the unit remains in/returns to `Joining` (B01-3).
6. Send `HOME\r\n` or `RESET\r\n` to either unit. Observe a full `SW_CPU_RESET` (U06).
7. Send `SOLO\r\n`, `ROCK\r\n`, `AGAIN\r\n` to verify single-player mode still works.

---

## Attachments

- Test log: `.tmp/CYD_RPS_command_test_20260622_001211.log`
- Build artifact: `.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`
- Release package: `dist/CYD_RPS_v0.2.1.zip`
- Source files involved:
  - `src/state_machine/ble_service.cpp`
  - `src/state_machine/app_state_machine.cpp`
  - Serial command dispatch layer
