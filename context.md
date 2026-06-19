# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.1.7  
**Workflow ID:** wvc_20260619_134606  
**Revision Type:** bug_fix  
**Release Date:** 2026-06-19  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS

---

## Bug-Fix Summary

This release fixes the JOIN-side BLE connection failure reported in v0.1.6 (`docs/BugReport_CYD_RPS_v0.1.6.md` §8.5).

After v0.1.6 made role negotiation symmetric, the JOIN still remained a peripheral advertiser while attempting to act as a central and connect to the HOST. The ESP32 NimBLE controller rejected the connection with `E NimBLEClient: Connection failed; status=13` (`BLE_ERR_CONN_REJ_RESOURCES`). Each failed attempt blocked for the default 30 s connect timeout, producing an apparent indefinite "Connecting" stall.

| Fix | Location | Details |
|-----|----------|---------|
| JOIN stops advertising before connecting | `src/state_machine/ble_service.cpp` | `BleService::connect_to_host()` calls `stop_advertising()` after creating the `NimBLEClient` and before the retry loop, removing the controller resource conflict. |
| Reduced NimBLE connect timeout | `src/state_machine/ble_service.h` / `ble_service.cpp` | Added `kConnectTimeoutSeconds = 5` and called `cli->setConnectTimeout(kConnectTimeoutSeconds)` before the first `connect()` attempt, reducing the per-attempt timeout from 30 s to 5 s. |
| Restart advertising on connection failure | `src/state_machine/ble_service.cpp` | If all retries fail, `connect_to_host()` calls `start_advertising()` before posting `EV_ERROR`, keeping the unit discoverable for a subsequent negotiation cycle. |
| In-file rationale comments | `src/state_machine/ble_service.cpp` / `ble_service.h` | Comments cite `docs/BugReport_CYD_RPS_v0.1.6.md` §8.5–8.6. |

This v0.1.7 fix preserves all v0.1.4–v0.1.6 fixes: GATT server start during `init()`, address-type capture in `peer_addr_type_`, dedicated BLE radio task, larger NimBLE host-task stack, heap guard, reduced LVGL memory, deferred discovery out of LVGL event context, thread-safe event queue, symmetric role negotiation using the public MAC, and the `WOKWI_SIMULATION` guard.

---

## Distribution

| Artifact | Path |
|----------|------|
| Release ZIP | `projects/CYD_RPS/dist/CYD_RPS_v0.1.7.zip` |
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
| GitHub release | PASS — release `v0.1.7` created at https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.1.7 with `firmware.bin` and `CYD_RPS_v0.1.7.zip` attached |

See `docs/qa_results.md` for the full QA report.

---

## GitHub Release

**GitHub release created successfully.**

- Repository: https://github.com/CSprinkle93065/CYD_RPS
- Release: https://github.com/CSprinkle93065/CYD_RPS/releases/tag/v0.1.7
- Tag: `v0.1.7`
- Attachments:
  - `firmware.bin`
  - `CYD_RPS_v0.1.7.zip`
- Local commit and tag were pushed to `origin/main`.

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
