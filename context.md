# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.2.2  
**Workflow ID:** wvc_20260621_164404  
**Revision Type:** bug_fix  
**Release Date:** 2026-06-21  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS  
**GitHub Release:** https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.2  
**Git Tag:** v0.2.2

---

## Bug-Fix Summary

This release fixes four issues discovered in v0.2.1 (see `docs/BugReport_CYD_RPS_v0.2.1.md`):

| Fix | ID | Location | Details |
|-----|----|----------|---------|
| BLE canonical MAC byte order | B01-1 | `src/state_machine/ble_service.cpp/h` | All public MACs are stored in canonical/display byte order so `NimBLEAddress(uint8_t[6], type)` receives canonical bytes and the controller sees the correct peer address. |
| Symmetric role resolution | B01-2 | `src/state_machine/app_state_machine.cpp/h`, `docs/state_machine.puml` | Start-screen auto-negotiation now uses `guard_local_mac_lower` to deterministically assign Host to the lower canonical public MAC and Join to the higher. |
| HOST command during join | B01-3 | `src/state_machine/app_state_machine.cpp/h`, `src/state_machine/ble_service.cpp/h` | Added `Joining --> HostWait` transition on `EV_HOST_GAME`. `BleService::become_host()` sets an `abort_connect_` flag so an in-progress JOIN connect loop aborts cleanly. |
| HOME/RESET soft reset | U06 | `docs/state_machine.puml`, `docs/state_machine_contract.json` | All `EV_RESET` transitions now target `Start / reset_and_return_start()` instead of `Halted / esp_restart()`. |

---

## Distribution Artifacts

| File | Path | Size |
|------|------|------|
| Firmware binary | `projects/CYD_RPS/.pio/build/esp32-2432s028r_cyd2usb/firmware.bin` | 960,416 B |
| Release package | `projects/CYD_RPS/dist/CYD_RPS_v0.2.2.zip` | 604,266 B |

Both files are attached to the GitHub release at https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.2.2.

---

## Flash Instructions

1. Build the hardware environment firmware (if not already built):
   ```bash
   pio run -e esp32-2432s028r_cyd2usb
   ```
2. Flash the unit with the v0.2.2 firmware:
   ```bash
   python execution/flash_cyd2usb.py --project CYD_RPS --port COM5
   ```
   Replace `COM5` with the appropriate serial port for your board.
3. Open a serial monitor at 115200 baud to verify the boot message:
   ```text
   CYD_RPS setup start
   CYD_RPS setup done
   STATE: Start
   ```

For two-board multiplayer verification, flash both units and follow the manual procedure in `docs/qa_results.md` §6.

---

## Quality Gate Summary

| Gate | Requirement | Status | Notes |
|------|-------------|--------|-------|
| G15.1 | Every automated test passes / zero collection errors | PASS | All executed automated tests passed (74 host + 6 Wokwi). |
| G15.2 | Every test verifies a behavioral outcome | PASS | Host tests assert state + action; Wokwi tests assert serial state transitions; reset tests confirm no CPU reboot. |
| G15.3 | Manual verification procedures documented for non-automated tests | PASS | Two-board BLE and bug-fix procedures documented in `docs/qa_results.md` §5 and §6. |
| G15.4 | Flash and RAM usage metrics captured | PASS | Both PlatformIO environments reported below. |
| G15.5 | Wokwi trace summary captured | PASS | Serial output and screen render captured in `docs/qa_results.md` §4. |

### Test Run Summary

- **Total test cases in test_plan.json:** 109
- **Executed automated cases:** 83
- **PASS:** 80
- **FAIL:** 0
- **BLOCKED:** 3
- **NOT EXECUTED:** 23

Host state-machine mirror: **74/74 PASS**.  
Wokwi automated serial_expect cases: **W01–W06 PASS**; W07–W09 blocked by simulator BLE limitation.  
Physical target / manual cases: **not executed** (no CYD2USB boards detected on COM5/COM6).

---

## Flash / RAM Metrics

| Environment | RAM Used | RAM Total | RAM % | Flash Used | Flash Total | Flash % |
|-------------|----------|-----------|-------|------------|-------------|---------|
| `esp32-2432s028r_cyd2usb` | 76,164 B | 327,680 B | 23.2% | 953,845 B | 1,310,720 B | 72.8% |
| `esp32-2432s028r_cyd2usb_wokwi` | 68,760 B | 327,680 B | 21.0% | 866,265 B | 1,310,720 B | 66.1% |

---

## Known Limitations

| Limitation | Details |
|------------|---------|
| BLE multiplayer requires physical hardware | NimBLE/BLE initialization is skipped under `WOKWI_SIMULATION` to avoid emulator watchdog issues. Multiplayer peer discovery, role negotiation, and peer move exchange require two physical CYD2USB v3 boards. |
| Wokwi touch not simulated | The Wokwi CLI does not support the `wokwi-xpt2046` touch part (LL-032). Touch-driven flows are validated by code review and require physical hardware. |
| Two-board connection verification is manual | The end-to-end two-board multiplayer connection flow must be verified manually because Wokwi does not emulate NimBLE reliably. |
| Scoreboard is RAM-only | Round history and scores persist across rounds but reset on every reboot. |

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
