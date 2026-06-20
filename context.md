# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.1.8  
**Workflow ID:** wvc_20260619_072600  
**Revision Type:** bug_fix  
**Release Date:** 2026-06-20  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS

---

## Bug-Fix Summary

This release fixes the BLE two-player discovery race reported in v0.1.7 (`docs/BugReport_CYD_RPS_v0.1.7.md` §7–§11.5).

After v0.1.7 stopped the JOIN's advertising at the start of `connect_to_host()`, the peer that should become HOST often had not yet discovered the JOIN and therefore never resolved its role or stopped scanning. When the JOIN tried to connect, the HOST was still scanning and could not accept the connection, producing repeated `E NimBLEClient: Connection failed; status=13` (`BLE_HS_ETIMEOUT`) every 5 s. The real issue is a discovery race, not the timeout value.

| Fix | Location | Details |
|-----|----------|---------|
| Guaranteed JOIN discovery window | `src/state_machine/ble_service.cpp` | After the JOIN resolves its role, `BleService::connect_to_host()` keeps advertising for `kJoinDiscoveryWindowMs` (2 s) before stopping advertising and initiating the first `connect()` attempt, giving the HOST time to discover the JOIN and resolve to HOST. |
| Per-retry discovery window | `src/state_machine/ble_service.cpp` | On each failed `connect()` attempt (except the last), the JOIN restarts advertising, waits `kJoinConnectInterRetryWindowMs` (2 s), stops advertising, then retries `connect()`. This repeatedly gives the HOST new chances to discover and stop scanning. |
| Stop advertising only immediately before connect | `src/state_machine/ble_service.cpp` | Advertising is kept running during every wait and is stopped only immediately before each `connect()` call. |
| Dedicated radio task ownership | `src/state_machine/ble_service.cpp` | All scan/advertise/connect operations remain inside the radio-task `CONNECT_TO_HOST` command handler; the wait is implemented with `vTaskDelay()` inside the radio task, never with `delay()` in `AppStateMachine::update()`. |
| Shortened advertising interval | `src/state_machine/ble_service.h` / `ble_service.cpp` | During peer search / role negotiation the JOIN advertises on a 20–30 ms interval (`kPeerSearchAdvMinInterval`/`kPeerSearchAdvMaxInterval` = 32/48 NimBLE units) so the HOST's scan window is more likely to hit an advertisement within each bounded window. |
| In-file rationale comments | `src/state_machine/ble_service.cpp` / `ble_service.h` | Every changed constant/timeout has an adjacent comment citing `docs/BugReport_CYD_RPS_v0.1.7.md` §9.1–§9.3 and §11.2 (LL-043). |

This v0.1.7 fix preserves all v0.1.4–v0.1.6 fixes: GATT server start during `init()`, address-type capture in `peer_addr_type_`, dedicated BLE radio task, larger NimBLE host-task stack, heap guard, reduced LVGL memory, deferred discovery out of LVGL event context, thread-safe event queue, symmetric role negotiation using the public MAC, and the `WOKWI_SIMULATION` guard.

---

## Distribution

| Artifact | Path |
|----------|------|
| Release ZIP | `projects/CYD_RPS/dist/CYD_RPS_v0.1.8.zip` |
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

Firmware size: 976,544 bytes.

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
```

---

## Known Limitations

| Limitation | Details |
|------------|---------|
| BLE multiplayer requires physical hardware | NimBLE/BLE initialization is skipped under `WOKWI_SIMULATION` to avoid emulator watchdog issues. Multiplayer peer discovery, role negotiation, and peer move exchange require two physical CYD2USB v3 boards. |
| Wokwi touch not simulated | The Wokwi CLI does not support the `wokwi-xpt2046` touch part (LL-032). Touch-driven flows are validated by code review and require physical hardware. |
| Two-board connection verification is manual | The end-to-end two-board multiplayer connection flow must be verified manually because no CYD2USB board is detected on the host USB ports and Wokwi does not emulate NimBLE reliably. It is documented as manual verification in `docs/qa_results.md` §5.1. |
| No score persistence | Round history and scores are RAM-only; they reset on every reboot. |
| Host tests use a Python model | The 40 host state-machine tests execute a Python mirror of the generated C++ state machine because no host C++ compiler is available and the firmware depends on Arduino/LVGL/NimBLE. The model is derived directly from `src/state_machine/state_machine_generated.cpp` and `docs/state_machine.puml`. |

---

## Quality Gate Summary

| Gate | Status |
|------|--------|
| Build (physical hardware environment) | PASS |
| Build (Wokwi simulation environment) | PASS |
| Host state-machine tests (40/40) | PASS |
| Static analysis (`pio check`) | PASS |
| Distribution ZIP complete | PASS |
| Bug-fix code review (JOIN connection resource conflict) | PASS |
| Integration code review | PASS |
| Wokwi smoke test | PASS |
| Physical target tests | MANUAL / HARDWARE-ONLY — no board attached |
| GitHub release | PASS — release `v0.1.8` created at https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.1.8 with `firmware.bin` and `CYD_RPS_v0.1.8.zip` attached |

See `docs/qa_results.md` for the full QA report.

---

## GitHub Release

**GitHub release created successfully.**

- Repository: https://github.com/CSprinkle93065/CYD_RPS
- Release: https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.1.8
- Tag: `v0.1.8`
- Attachments:
  - `firmware.bin`
  - `CYD_RPS_v0.1.8.zip`
- Local commit and tag were pushed to `origin/main`.

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
