# CYD_RPS — Release Context

**Project:** CYD_RPS  
**Version:** 0.1.6  
**Workflow ID:** wvc_20260619_034654  
**Revision Type:** bug_fix  
**Release Date:** 2026-06-19  
**Board Version:** CYD2USB_v3 (ESP32-2432S028R)  
**Target Environment:** hardware  
**Branch:** main  
**GitHub Repository:** https://github.com/CSprinkle93065/CYD_RPS

---

## Bug-Fix Summary

This release fixes the two-player multiplayer connection failure reported in v0.1.5 (`docs/BugReport_CYD_RPS_v0.1.5.md`).

The v0.1.5 release captured the peer BLE address type from scan advertisements and used that type in `NimBLEClient::connect()`, which resolved the immediate `E NimBLEClient: Connection failed; status=13` caused by a public/random address mismatch. However, two-board multiplayer still failed because the JOIN/HOST role-negotiation sequence was asymmetric: the JOIN stopped advertising when it resolved its role, so the HOST never discovered the JOIN and eventually timed out to standalone gameplay; the JOIN then attempted to connect to a HOST that was not ready.

| Fix | Location | Details |
|-----|----------|---------|
| JOIN keeps advertising during role negotiation | `src/state_machine/ble_service.cpp` | `BleService::become_join()` calls `stop_scanning()` but deliberately does **not** call `stop_advertising()`, so the peer that becomes HOST can still discover the JOIN and resolve its own role. |
| JOIN retries the HOST connection | `src/state_machine/ble_service.cpp` | `connect_to_host()` loops up to `kConnectRetries` (4) attempts with `kConnectRetryDelayMs` (250 ms) backoff before posting `EV_ERROR`. |
| Public MAC embedded in advertisement payload | `src/state_machine/ble_service.cpp` | `start_advertising()` adds manufacturer data with `kManufacturerCompanyId` (0xFFFF) followed by the 6-byte public MAC (`NimBLEDevice::getAddress()`). |
| Peer public MAC extracted from scan payload | `src/state_machine/ble_service.cpp` | `RpsAdvertisedDeviceCallbacks::onResult()` parses the manufacturer data and passes the public MAC to `on_peer_found()`. |
| Role resolution uses stable public MAC | `src/state_machine/ble_service.cpp` | `resolve_role()` compares local/peer public MACs when available; otherwise falls back to the random advertisement address. |
| HOST does not regenerate random address | `src/state_machine/ble_service.cpp` | `become_host()` calls `start_advertising()`, which returns immediately if advertising is already active. |
| In-file comments referencing bug report | `src/state_machine/ble_service.cpp` / `ble_service.h` | Comments cite `docs/BugReport_CYD_RPS_v0.1.5.md` §6.1–6.4 and §8.1–8.4. |

This v0.1.6 fix preserves all v0.1.4 and v0.1.5 fixes: GATT server start during `init()`, address-type capture in `peer_addr_type_`, dedicated BLE radio task, larger NimBLE host-task stack, heap guard, reduced LVGL memory, deferred discovery out of LVGL event context, and thread-safe event queue.

---

## Distribution

| Artifact | Path |
|----------|------|
| Release ZIP | `projects/CYD_RPS/dist/CYD_RPS_v0.1.6.zip` |
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

Firmware size: 976,496 bytes.

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
| Two-board connection verification is manual | The end-to-end two-board multiplayer connection flow must be verified manually because no CYD2USB board is detected on the host USB ports and Wokwi does not emulate NimBLE reliably. It is documented as manual verification in `docs/qa_results.md` §6.1. |
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
| Bug-fix code review (role-negotiation root cause) | PASS |
| Integration code review | PASS |
| Wokwi smoke test | SKIPPED — `WOKWI_CLI_TOKEN` not exported in the current shell; reference trace from prior run passes |
| Physical target tests | MANUAL / HARDWARE-ONLY — no board attached |
| GitHub release | SKIPPED — repository `CSprinkle93065/CYD_RPS` does not exist on GitHub and no git remote is configured |

See `docs/qa_results.md` for the full QA report.

---

## GitHub Release

**GitHub release skipped — repository does not exist and no remote is configured.**

- Inferred repository `CSprinkle93065/CYD_RPS` does not exist on GitHub (`gh repo view CSprinkle93065/CYD_RPS` returned "Could not resolve to a Repository").
- The local git repository has no remotes configured (`git remote -v` returns empty).
- The `gh` CLI is installed and authenticated as `CSprinkle93065`, but a release cannot be created until the repository is created and a remote is added.
- Local artifacts (`firmware.bin` and `CYD_RPS_v0.1.6.zip`) are available under `projects/CYD_RPS/dist/` for manual distribution.
- To create a release in the future, create the repository `https://github.com/CSprinkle93065/CYD_RPS`, add it as a remote, push the project, and re-run the Release Agent.

---

## Fork Branches Registry

<!-- Placeholder: register feature/fix branches below when forking from main. -->

| Branch | Purpose | Owner | Status |
|--------|---------|-------|--------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ |
