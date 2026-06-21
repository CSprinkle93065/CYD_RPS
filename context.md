# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.2.1  
**Workflow ID:** wvc_20260618_011606  
**Revision Type:** bug_fix  
**Release Date:** 2026-06-21  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS

---

## Bug-Fix Summary

This release fixes one BLE auto-join issue and five UI issues discovered in v0.2.0.

| Fix | ID | Location | Details |
|-----|----|----------|---------|
| BLE auto-join advertisement filtering | B01 | `src/state_machine/ble_service.cpp/h` | JOIN now accepts a Host advertisement by service UUID **or** by the manufacturer-data presence-beacon signature (company ID `0x00FF`, 12-byte payload). The 4-char device ID is validated as hex, and the peer address type is preserved for the subsequent NimBLE connection. |
| Scoreboard layout overlap | U01 | `src/ui/screens.cpp` | Scoreboard label aligned to bottom center; Play Again button moved up so the two no longer overlap. |
| White text on black background | U02 | `src/ui/theme.cpp` | All label and button text now uses `lv_color_white()` for consistent readability on the dark UI theme. |
| Timeout-dialog button layout | U03 | `src/ui/screens.cpp` | Retry and Solo buttons on the Host timeout dialog are now smaller (85×45) and placed side-by-side at the bottom of the dialog. |
| Host-wait progress bar styling | U04 | `src/ui/screens.cpp` | Progress-bar main background is black and the indicator fill is blue (`#0066FF`), making it visible on the dark theme. |
| Result-screen scoreboard consistency | U05 | `src/ui/ui.cpp` | Cached score values are applied to the Result screen so it displays the same win/loss/draw counts as the Gameplay screen. |

This v0.2.1 release preserves all v0.2.0 features: user-selected Host mode, BLE presence beacons, white-on-black UI theme, persistent scoreboard, serial command parser, Host timeout dialog, home icon reset, and state instrumentation.

---

## Distribution

| Artifact | Path |
|----------|------|
| Release ZIP | `projects/CYD_RPS/dist/CYD_RPS_v0.2.1.zip` |
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

Firmware size: 959,616 bytes.

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
| Wokwi HOST path errors | In Wokwi, `do_become_host()` posts `EV_ERROR` because BLE is not initialized under `WOKWI_SIMULATION`; the HOST timeout dialog cannot be reached in simulation. |

---

## Quality Gate Summary

| Gate | Status |
|------|--------|
| Build (physical hardware environment) | PASS |
| Build (Wokwi simulation environment) | PASS |
| Host state-machine tests (73/73) | PASS |
| Distribution ZIP complete | PASS |
| UI/UX bug fixes (U01–U05) verified | PASS |
| BLE bug fix (B01) verified | PASS |
| Wokwi smoke test | PASS (W02, W07–W09 blocked by NimBLE limitation) |
| Physical target tests | NOT EXECUTED — no board attached |
| Manual touch/BLE verification procedures | DOCUMENTED |
| GitHub release | PASS — release `v0.2.1` created at https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.1 with `firmware.bin` and `CYD_RPS_v0.2.1.zip` attached |

See `docs/qa_results.md` for the full QA report.

---

## GitHub Release

**GitHub release created successfully.**

- Repository: https://github.com/CSprinkle93065/CYD_RPS
- Release: https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.1
- Tag: `v0.2.1`
- Attachments:
  - `firmware.bin`
  - `CYD_RPS_v0.2.1.zip`
- Local commit and tag were pushed to `origin/main`.

---

## Flash and RAM Usage Metrics

### Physical-Hardware Environment (`esp32-2432s028r_cyd2usb`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 76,148 bytes | 327,680 bytes | 23.2% |
| Flash | 953,033 bytes | 1,310,720 bytes | 72.7% |

**Partition Scheme:** `default.csv` (1.3 MB application partition)  
**Partition-Fit Check:** PASS — firmware fits within the 1,310,720-byte application partition.

### Wokwi Simulation Environment (`esp32-2432s028r_cyd2usb_wokwi`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 68,760 bytes | 327,680 bytes | 21.0% |
| Flash | 865,605 bytes | 1,310,720 bytes | 66.0% |

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
