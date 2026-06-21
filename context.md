# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Revision Type:** minor_revision  
**Release Date:** 2026-06-21  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS

---

## Minor-Revision Summary

This release adds user-selected Host mode, BLE presence beacons, a white-on-black UI theme, persistent scoreboard, and a serial command parser to CYD_RPS.

| Feature | Location | Details |
|---------|----------|---------|
| User-selected Host mode | `src/state_machine/app_state_machine.cpp`, `src/ui/screens.cpp` | Player can tap "Host Game" to enter `HostWait` and broadcast a BLE presence beacon while scanning for peers. |
| BLE presence beacons & peer discovery | `src/state_machine/ble_service.cpp/h` | HOST broadcasts a connectable advertisement; JOIN scans and resolves role conflicts using public MAC address (lower MAC becomes HOST). |
| White-on-black UI theme | `src/ui/theme.cpp/h`, `src/ui/screens.cpp` | Inverts previous dark-on-light scheme for improved readability on the CYD2USB display. |
| Persistent scoreboard | `src/state_machine/app_state_machine.cpp`, `src/state_machine/game_logic.h` | Tracks wins, losses, and draws across multiple rounds; resets on boot. |
| Serial command parser | `src/state_machine/serial_command_handler.cpp/h` | Recognizes `HOST`, `SOLO`, `ROCK`, `PAPER`, `SCISSORS`, `AGAIN`, `RESET`, `HOME`, and unknown/malformed commands; posts corresponding events to the state machine. |
| Host timeout dialog | `src/state_machine/app_state_machine.cpp`, `src/ui/screens.cpp` | If no peer is found within ~15 s, a Retry/Solo dialog is shown. |
| Home icon reset | `src/ui/screens.cpp`, `src/ui/touch_mapping.cpp/h` | A home icon in the top-right corner posts `EV_RESET` from any screen. |
| State instrumentation | `src/state_machine/app_state_machine.cpp` | Emits `STATE:`, `MODE:`, and `SCORE:` markers over serial for QA observability. |

This v0.2.0 release preserves all v0.1.4–v0.1.9 fixes and infrastructure: GATT server start during `init()`, address-type capture, dedicated BLE radio task, larger NimBLE host-task stack, heap guard, reduced LVGL memory, deferred discovery, thread-safe event queue, symmetric role negotiation, bounded JOIN discovery/retry windows, short peer-search advertising interval, stop-advertise-only-before-connect, and the `WOKWI_SIMULATION` guard.

---

## Distribution

| Artifact | Path |
|----------|------|
| Release ZIP | `projects/CYD_RPS/dist/CYD_RPS_v0.2.0.zip` |
| Firmware binary | `firmware.bin` (inside ZIP and at `projects/CYD_RPS/.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`) |
| Build configuration | `platformio.ini` (inside ZIP) |
| Flash scripts | `flash_esptool.bat`, `flash_esptool.sh`, `flash_cyd_rps.bat` (inside ZIP) |
| Flash instructions | `README.md` (inside ZIP) |

ZIP contents verified:
- `firmware.bin`
- `platformio.ini`
- `README.md`
- `flash_esptool.bat`
- `flash_esptool.sh`
- `flash_cyd_rps.bat`

Firmware size: 958,800 bytes.

---

## Flash Instructions

### Using the workspace flash helper (recommended for physical hardware)

```bash
python execution/flash_cyd2usb.py --project CYD_RPS --port COM5
```

Because `platformio.ini` defaults to the physical-hardware environment, the `--env` argument is optional. To flash the Wokwi simulation binary explicitly, use:

```bash
python execution/flash_cyd2usb.py --project CYD_RPS --env esp32-2432s028r_cyd2usb_wokwi --port COM5
```

### Using esptool directly from the extracted ZIP

Windows:

```batch
flash_esptool.bat COM5
```

Linux / macOS:

```bash
./flash_esptool.sh /dev/ttyUSB0
```

### Expected serial boot marker

After flashing, open the serial monitor at `115200` baud and reset the board. You should see:

```text
CYD_RPS setup start
CYD_RPS setup done
STATE: Start
```

---

## Known Limitations

| Limitation | Details |
|------------|---------|
| BLE multiplayer requires physical hardware | NimBLE/BLE initialization is skipped under `WOKWI_SIMULATION` to avoid emulator watchdog issues. Multiplayer peer discovery, role negotiation, and peer move exchange require two physical CYD2USB v3 boards. |
| Wokwi touch not simulated | The Wokwi CLI does not support the `wokwi-xpt2046` touch part (LL-032). Touch-driven flows are validated by code review and require physical hardware. |
| Two-board connection verification is manual | The end-to-end two-board multiplayer connection flow must be verified manually because no CYD2USB board is detected on the host USB ports and Wokwi does not emulate NimBLE reliably. It is documented as manual verification in `docs/qa_results.md` §4. |
| Scoreboard is RAM-only | Round history and scores persist across rounds but reset on every reboot. |
| Host tests use a Python model | The 73 host state-machine tests execute a Python mirror of the generated C++ state machine because no host C++ compiler is available and the firmware depends on Arduino/LVGL/NimBLE. The model is derived directly from `src/state_machine/state_machine_generated.cpp` and `docs/state_machine.puml`. |
| Wokwi HOST path errors | In Wokwi, `do_become_host()` posts `EV_ERROR` because BLE is not initialized; the HOST timeout dialog cannot be reached in simulation. |

---

## Quality Gate Summary

| Gate | Status |
|------|--------|
| Build (physical hardware environment) | PASS |
| Build (Wokwi simulation environment) | PASS |
| Host state-machine tests (73/73) | PASS |
| Distribution ZIP complete | PASS |
| Integration code review | PASS |
| Wokwi smoke test | PASS (W07–W09 blocked by NimBLE limitation) |
| Physical target tests | NOT EXECUTED — no board attached |
| Manual touch/BLE verification procedures | DOCUMENTED |
| GitHub release | PASS — release `v0.2.0` created at https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.0 with `firmware.bin` and `CYD_RPS_v0.2.0.zip` attached |

See `docs/qa_results.md` for the full QA report.

---

## GitHub Release

**GitHub release created successfully.**

- Repository: https://github.com/CSprinkle93065/CYD_RPS
- Release: https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.0
- Tag: `v0.2.0`
- Attachments:
  - `firmware.bin`
  - `CYD_RPS_v0.2.0.zip`
- Local commit and tag were pushed to `origin/main`.

---

## Flash and RAM Usage Metrics

### Physical-Hardware Environment (`esp32-2432s028r_cyd2usb`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 76,140 bytes | 327,680 bytes | 23.2% |
| Flash | 952,225 bytes | 1,310,720 bytes | 72.6% |

**Partition Scheme:** `default.csv` (1.3 MB application partition)  
**Partition-Fit Check:** PASS — firmware fits within the 1,310,720-byte application partition.

### Wokwi Simulation Environment (`esp32-2432s028r_cyd2usb_wokwi`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 68,752 bytes | 327,680 bytes | 21.0% |
| Flash | 864,833 bytes | 1,310,720 bytes | 66.0% |

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
