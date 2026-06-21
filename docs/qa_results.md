# CYD_RPS v0.2.1 QA Results

**Workflow ID:** wvc_20260618_011606  
**Project:** CYD_RPS  
**Version:** 0.2.1  
**Revision Type:** minor_revision  
**QA Agent Run Date:** 2026-06-21  
**Status:** PASS (with Wokwi BLE and hardware-only limitations documented)

## Executive Summary

| Category | Count | Status |
|----------|-------|--------|
| Host state-machine tests | 73 | **73 PASS / 0 FAIL** |
| Wokwi simulator tests | 9 | **5 PASS / 4 SKIP** |
| Physical-hardware target tests | 9 | **9 SKIP** — no CYD2USB board connected |
| Manual touch/BLE tests | 14 | **14 SKIP** — hardware/touch/BLE peer required |
| Build — physical environment | 1 | **SUCCESS** |
| Build — Wokwi environment | 1 | **SUCCESS** |
| `pio test` harness | — | **NOT APPLICABLE** — project uses `tests/host/test_state_machine.py` |

## 1. Host State-Machine Tests (`tests/host/test_state_machine.py`)

**Command:** `python tests/host/test_state_machine.py`  
**Result:** 73 passed, 0 failed out of 73 `host_assert` tests

> **Note on `pio test`:** The project does not contain a PlatformIO unit-test directory (`test/`). Running `pio test -e esp32-2432s028r_cyd2usb` raises `TestDirNotExistsError`. The executable host harness is the Python model in `tests/host/test_state_machine.py`, which is the runner referenced by `test_plan.json` for all `host_assert` tests.

| ID | Title | Result | Evidence |
|----|-------|--------|----------|
| H00 | Initial pseudo-state lands in Boot | PASS | Initial state is Boot; no action invoked |
| H01 | Boot -> Start on EV_BOOT_DONE [init_ok] | PASS | Boot -> Start; actions=[`app_init_success`] |
| H02 | Boot -> Error on EV_ERROR [ble_init_failed] | PASS | Boot -> Error; actions=[`app_on_error_ble`] |
| H03 | Boot -> Halted on EV_ERROR [hw_init_failed] | PASS | Boot -> Halted; actions=[`app_on_error_hw`] |
| H04 | Start -> HostWait on EV_HOST_GAME | PASS | Start -> HostWait; actions=[`on_host_game`] |
| H05 | Start -> Joining on EV_HOST_FOUND [host_mac_valid] | PASS | Start -> Joining; actions=[`on_conflict_become_join`] |
| H06 | Start -> Gameplay on EV_SOLO | PASS | Start -> Gameplay; actions=[`on_solo`] |
| H07 | Start -> Error on EV_ERROR | PASS | Start -> Error; actions=[`app_on_error`] |
| H08 | Start -> Halted on EV_RESET | PASS | Start -> Halted; actions=[`esp_restart`] |
| H09 | HostWait -> HostTimeoutDialog on EV_HOST_TIMEOUT | PASS | HostWait -> HostTimeoutDialog; actions=[`on_host_timeout`] |
| H10 | HostWait -> Joining on EV_HOST_FOUND [peer_host_seen] | PASS | HostWait -> Joining; actions=[`on_conflict_become_join`] |
| H11 | HostWait -> Gameplay on EV_CONNECTED | PASS | HostWait -> Gameplay; actions=[`on_peer_connected`] |
| H12 | HostWait -> Start on EV_CANCEL_HOST | PASS | HostWait -> Start; actions=[`on_cancel_host`] |
| H13 | HostWait -> Halted on EV_RESET | PASS | HostWait -> Halted; actions=[`esp_restart`] |
| H14 | HostWait -> Error on EV_ERROR | PASS | HostWait -> Error; actions=[`app_on_error`] |
| H15 | HostTimeoutDialog -> HostWait on EV_HOST_RETRY | PASS | HostTimeoutDialog -> HostWait; actions=[`on_host_retry`] |
| H16 | HostTimeoutDialog -> Gameplay on EV_SOLO | PASS | HostTimeoutDialog -> Gameplay; actions=[`on_host_solo`] |
| H17 | HostTimeoutDialog -> Start on EV_CANCEL_HOST | PASS | HostTimeoutDialog -> Start; actions=[`on_cancel_host`] |
| H18 | HostTimeoutDialog -> Halted on EV_RESET | PASS | HostTimeoutDialog -> Halted; actions=[`esp_restart`] |
| H19 | Joining -> Gameplay on EV_CONNECTED | PASS | Joining -> Gameplay; actions=[`on_peer_connected`] |
| H20 | Joining -> Start on EV_CONNECT_FAILED [retries_exhausted] | PASS | Joining -> Start; actions=[`on_join_failed`] |
| H21 | Joining -> Halted on EV_RESET | PASS | Joining -> Halted; actions=[`esp_restart`] |
| H22 | Joining -> Error on EV_ERROR | PASS | Joining -> Error; actions=[`app_on_error`] |
| H23-ROCK | Gameplay -> Evaluating on EV_MOVE_ROCK [mode_single] | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_rock`] |
| H24-ROCK | Gameplay -> Evaluating on EV_MOVE_ROCK [mode_multi && peer_move_received] | PASS | Gameplay -> Evaluating; actions=[`on_local_move_rock_complete`] |
| H25-ROCK | Gameplay -> Gameplay on EV_MOVE_ROCK [mode_multi && !peer_move_received] | PASS | Gameplay -> Gameplay; actions=[`on_local_move_rock_pending`] |
| H23-PAPER | Gameplay -> Evaluating on EV_MOVE_PAPER [mode_single] | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_paper`] |
| H24-PAPER | Gameplay -> Evaluating on EV_MOVE_PAPER [mode_multi && peer_move_received] | PASS | Gameplay -> Evaluating; actions=[`on_local_move_paper_complete`] |
| H25-PAPER | Gameplay -> Gameplay on EV_MOVE_PAPER [mode_multi && !peer_move_received] | PASS | Gameplay -> Gameplay; actions=[`on_local_move_paper_pending`] |
| H23-SCISSORS | Gameplay -> Evaluating on EV_MOVE_SCISSORS [mode_single] | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_scissors`] |
| H24-SCISSORS | Gameplay -> Evaluating on EV_MOVE_SCISSORS [mode_multi && peer_move_received] | PASS | Gameplay -> Evaluating; actions=[`on_local_move_scissors_complete`] |
| H25-SCISSORS | Gameplay -> Gameplay on EV_MOVE_SCISSORS [mode_multi && !peer_move_received] | PASS | Gameplay -> Gameplay; actions=[`on_local_move_scissors_pending`] |
| H44-HOST | Serial command parser posts EV_HOST_GAME on receiving 'HOST' | PASS | Start -> HostWait; actions=[`on_host_game`] |
| H44-SOLO | Serial command parser posts EV_SOLO on receiving 'SOLO' | PASS | Start -> Gameplay; actions=[`on_solo`] |
| H44-ROCK | Serial command parser posts EV_MOVE_ROCK on receiving 'ROCK' | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_rock`] |
| H44-PAPER | Serial command parser posts EV_MOVE_PAPER on receiving 'PAPER' | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_paper`] |
| H44-SCISSORS | Serial command parser posts EV_MOVE_SCISSORS on receiving 'SCISSORS' | PASS | Gameplay -> Evaluating; actions=[`on_singleplayer_move_scissors`] |
| H44-AGAIN | Serial command parser posts EV_PLAY_AGAIN on receiving 'AGAIN' | PASS | Result -> Gameplay; actions=[`start_new_round`] |
| H44-RESET | Serial command parser posts EV_RESET on receiving 'RESET' | PASS | Start -> Halted; actions=[`esp_restart`] |
| H44-HOME | Serial command parser posts EV_RESET on receiving 'HOME' | PASS | Start -> Halted; actions=[`esp_restart`] |
| H46-WIN | Scoreboard wins increments after WIN outcome | PASS | Evaluating -> Result; actions=[`evaluate_and_show_result`] |
| H46-LOSE | Scoreboard losses increments after LOSE outcome | PASS | Evaluating -> Result; actions=[`evaluate_and_show_result`] |
| H46-DRAW | Scoreboard draws increments after DRAW outcome | PASS | Evaluating -> Result; actions=[`evaluate_and_show_result`] |
| H49-Start | Home icon reset from Start posts EV_RESET and halts | PASS | Start -> Halted; actions=[`esp_restart`] |
| H49-HostWait | Home icon reset from HostWait posts EV_RESET and halts | PASS | HostWait -> Halted; actions=[`esp_restart`] |
| H49-HostTimeoutDialog | Home icon reset from HostTimeoutDialog posts EV_RESET and halts | PASS | HostTimeoutDialog -> Halted; actions=[`esp_restart`] |
| H49-Gameplay | Home icon reset from Gameplay posts EV_RESET and halts | PASS | Gameplay -> Halted; actions=[`esp_restart`] |
| H49-Evaluating | Home icon reset from Evaluating posts EV_RESET and halts | PASS | Evaluating -> Halted; actions=[`esp_restart`] |
| H49-Result | Home icon reset from Result posts EV_RESET and halts | PASS | Result -> Halted; actions=[`esp_restart`] |
| H49-Disconnected | Home icon reset from Disconnected posts EV_RESET and halts | PASS | Disconnected -> Halted; actions=[`esp_restart`] |
| H49-Error | Home icon reset from Error posts EV_RESET and halts | PASS | Error -> Halted; actions=[`esp_restart`] |
| H26 | Gameplay -> Evaluating on EV_PEER_MOVE_RECEIVED [local_move_chosen] | PASS | Gameplay -> Evaluating; actions=[`on_peer_move_complete`] |
| H27 | Gameplay -> Gameplay on EV_PEER_MOVE_RECEIVED [!local_move_chosen] | PASS | Gameplay -> Gameplay; actions=[`on_peer_move_pending`] |
| H28 | Gameplay -> Disconnected on EV_DISCONNECTED | PASS | Gameplay -> Disconnected; actions=[`on_peer_disconnected`] |
| H29 | Gameplay -> Halted on EV_RESET | PASS | Gameplay -> Halted; actions=[`esp_restart`] |
| H30 | Gameplay -> Error on EV_ERROR | PASS | Gameplay -> Error; actions=[`app_on_error`] |
| H31 | Evaluating -> Result on EV_EVALUATE | PASS | Evaluating -> Result; actions=[`evaluate_and_show_result`] |
| H32 | Evaluating -> Halted on EV_RESET | PASS | Evaluating -> Halted; actions=[`esp_restart`] |
| H33 | Evaluating -> Error on EV_ERROR | PASS | Evaluating -> Error; actions=[`app_on_error`] |
| H34 | Result -> Gameplay on EV_PLAY_AGAIN | PASS | Result -> Gameplay; actions=[`start_new_round`] |
| H35 | Result -> Disconnected on EV_DISCONNECTED | PASS | Result -> Disconnected; actions=[`on_peer_disconnected`] |
| H36 | Result -> Halted on EV_RESET | PASS | Result -> Halted; actions=[`esp_restart`] |
| H37 | Result -> Error on EV_ERROR | PASS | Result -> Error; actions=[`app_on_error`] |
| H38 | Disconnected -> Start on EV_RETRY | PASS | Disconnected -> Start; actions=[`reset_and_return_start`] |
| H39 | Disconnected -> Halted on EV_RESET | PASS | Disconnected -> Halted; actions=[`esp_restart`] |
| H40 | Disconnected -> Error on EV_ERROR | PASS | Disconnected -> Error; actions=[`app_on_error`] |
| H41 | Error -> Start on EV_RETRY | PASS | Error -> Start; actions=[`reset_and_return_start`] |
| H42 | Error -> Halted on EV_RESET | PASS | Error -> Halted; actions=[`esp_restart`] |
| H43 | Error -> Halted on EV_ERROR [fatal] | PASS | Error -> Halted; actions=[`app_on_error_fatal`] |
| H45 | Serial parser ignores unknown or malformed commands | PASS | Initial state is Start; no action invoked |
| H47 | Scoreboard persists across multiple rounds | PASS | Result -> Gameplay; actions=[`start_new_round`] |
| H48 | Scoreboard resets to zero on boot | PASS | Boot -> Boot; score-test; actions=[] |
| H50 | Cancel hosting returns to Start and restarts beacon/scan | PASS | HostWait -> Start; actions=[`on_cancel_host`] |

## 2. Wokwi Simulation Tests

**Build:** `pio run -e esp32-2432s028r_cyd2usb_wokwi` — SUCCESS  
**Smoke scenario:** `wokwi-cli --diagram-file wokwi/diagram.json --scenario wokwi/smoke.yaml` — SUCCESS  
**Clean SOLO-path scenario:** `wokwi-cli --diagram-file wokwi/diagram.json --scenario wokwi/qa_solo_path.yaml --timeout 15000` — SUCCESS

| ID | Title | Result | Observed Evidence |
|----|-------|--------|-------------------|
| W01 | Boot succeeds and firmware reaches Start state in Wokwi | PASS | `CYD_RPS setup start`, `CYD_RPS setup done`, and `STATE: Start` observed |
| W02 | Serial HOST command reaches HostWait in Wokwi | SKIP | `STATE: HostWait` is emitted, but the device transitions to `STATE: Error` immediately afterward because NimBLE is not initialized under `WOKWI_SIMULATION`; must be verified on physical hardware |
| W03 | Serial SOLO command enters single-player Gameplay in Wokwi | PASS | Clean SOLO path: `MODE: SINGLE_PLAYER` and `STATE: Gameplay` observed |
| W04 | Serial ROCK/PAPER/SCISSORS advances single-player round to Result in Wokwi | PASS | Clean SOLO path: `STATE: Evaluating` → `SCORE:` → `STATE: Result` |
| W05 | Serial AGAIN starts a new single-player round in Wokwi | PASS | Clean SOLO path: `SERIAL: AGAIN` → `STATE: Gameplay` |
| W06 | Serial RESET reboots from any screen in Wokwi | PASS | `SW_CPU_RESET` and repeated boot banner observed |
| W07 | Host timeout dialog appears after 15 s without peer in Wokwi | SKIP | Requires a stable `HostWait` state; blocked by Wokwi NimBLE limitation (see W02) |
| W08 | Host timeout dialog Retry returns to HostWait in Wokwi | SKIP | Requires reaching `HostTimeoutDialog`; blocked by Wokwi NimBLE limitation |
| W09 | Host timeout dialog Solo enters single-player Gameplay in Wokwi | SKIP | Requires reaching `HostTimeoutDialog`; blocked by Wokwi NimBLE limitation |

### 2.1 Wokwi Trace Summary

Key observations from the live simulation:

1. Power-on reset (`POWERON_RESET`, `SPI_FAST_FLASH_BOOT`).
2. Firmware prints `CYD_RPS setup start`.
3. `CYD_RPS: Wokwi simulation - BLE stack skipped` confirms the `WOKWI_SIMULATION` code path in `main.cpp`.
4. `CYD_RPS setup done` — boot/primary-screen marker present.
5. `STATE: Start` is emitted on first render after boot.
6. Clean SOLO path emits the full marker sequence:
   ```
   SERIAL: SOLO
   MODE: SINGLE_PLAYER
   STATE: Gameplay
   SERIAL: ROCK
   STATE: Evaluating
   SCORE: W0 L0 D1
   STATE: Result
   SERIAL: AGAIN
   STATE: Gameplay
   ```
7. HOST path emits `MODE: MULTI_PLAYER` and `STATE: HostWait`, then immediately `STATE: Error` because Wokwi does not emulate NimBLE and `main.cpp` skips BLE initialization under `WOKWI_SIMULATION`.
8. `RESET` triggers `SW_CPU_RESET`; the boot banner repeats and the device recovers to `CYD_RPS setup done`.
9. Wokwi CLI reports `unknown-part-type` for `wokwi-xpt2046` (touch controller), confirming that touch-driven tests must be run on physical hardware.

## 3. Physical-Hardware Target Tests

**Status:** SKIP — HARDWARE-ONLY  
**Reason:** `pio device list` shows only Bluetooth serial ports (COM3, COM4). No CYD2USB/CH340 board is connected, so the target firmware cannot be flashed or monitored.

| ID | Title | Automation Status |
|----|-------|-------------------|
| T01 | Target boot succeeds and reaches Start screen | SKIP (hardware-only) |
| T02 | Boot -> Error when BLE initialization fails on target | SKIP (hardware-only) |
| T03 | Serial HOST command enters HostWait on target hardware | SKIP (hardware-only) |
| T04 | Serial SOLO command enters single-player Gameplay on target hardware | SKIP (hardware-only) |
| T05 | Serial ROCK/PAPER/SCISSORS completes a single-player round on target hardware | SKIP (hardware-only) |
| T06 | Scoreboard updates after a win/loss/draw on target hardware | SKIP (hardware-only) |
| T07 | Serial AGAIN starts a new round preserving scoreboard on target hardware | SKIP (hardware-only) |
| T08 | Serial RESET reboots from Gameplay on target hardware | SKIP (hardware-only) |
| T09 | Host timeout dialog appears after 15 s without peer on target hardware | SKIP (hardware-only) |

## 4. Manual Verification Procedures

The following tests require physical touch input and/or two-board BLE interaction and cannot be automated in Wokwi.

### 4.1 Touch-Driven Flows

| ID | Title | Manual Verification Procedure |
|----|-------|-------------------------------|
| T10 | Host timeout dialog Retry restarts HostWait on target hardware | With the device showing the Host timeout dialog (SCR_HostWait with Retry/Solo overlay), tap the Retry button. Verify the dialog dismisses and the screen returns to SCR_HostWait with the progress bar refilled. |
| T11 | Host timeout dialog Solo enters single-player Gameplay on target hardware | With the device showing the Host timeout dialog, tap the Solo button. Verify the dialog dismisses and the screen switches to SCR_Gameplay in single-player mode. |
| T12 | Cancel hosting returns to Start on target hardware | With the device showing SCR_HostWait, tap the Cancel button. Verify the screen returns to SCR_Start and the presence beacon/scan resumes. |
| M01 | Tap Host Game button enters HostWait | On SCR_Start, tap the Host Game button. Verify the screen switches to SCR_HostWait with 'Hosting Game' title, device ID, progress bar, and Cancel button. |
| M02 | Tap Solo button enters single-player Gameplay | On SCR_Start, tap the Solo button. Verify the screen switches to SCR_Gameplay with score label 'W:0 L:0 D:0' and move buttons enabled. |
| M03 | Tap a move button completes a single-player round | In single-player SCR_Gameplay, tap Rock, Paper, or Scissors. Verify the selected move is recorded, move buttons disable, and the screen transitions to SCR_Result with the outcome. |
| M04 | Scoreboard updates correctly across single-player rounds | Play multiple single-player rounds and observe the score label. Verify wins increment W, losses increment L, and draws increment D. |
| M05 | Tap Play Again starts a new single-player round | After a single-player round on SCR_Result, tap Play Again. Verify the screen returns to SCR_Gameplay, move buttons re-enable, and the scoreboard is unchanged. |
| M06 | Home icon reset from Start reboots device | On SCR_Start, tap the home icon in the top-right corner. Verify the device reboots and returns to SCR_Start. |
| M07 | Home icon reset from HostWait reboots device | On SCR_HostWait, tap the home icon in the top-right corner. Verify the device reboots and returns to SCR_Start. |
| M08 | Home icon reset from Gameplay reboots device | On SCR_Gameplay, tap the home icon in the top-right corner. Verify the device reboots and returns to SCR_Start. |
| M09 | Home icon reset from Result reboots device | On SCR_Result, tap the home icon in the top-right corner. Verify the device reboots and returns to SCR_Start. |
| M10 | Home icon reset from Error/Disconnected reboots device | On SCR_Error, tap the home icon in the top-right corner. Verify the device reboots and returns to SCR_Start. |

### 4.2 Two-Board BLE Peer Connection

| ID | Title | Manual Verification Procedure |
|----|-------|-------------------------------|
| M11 | Two-board BLE: Host device enters HostWait and Joiner auto-connects | Power cycle both devices and wait for SCR_Start on each. On Device A tap Host Game; place Device B on SCR_Start within BLE range. Verify both devices transition to SCR_Gameplay in multiplayer mode (serial logs `MODE: MULTI_PLAYER` and `STATE: Gameplay` on both). |
| M12 | Two-board BLE: complete a multiplayer round with both moves | With both devices in multiplayer SCR_Gameplay, each player taps a different move button. Verify both devices show SCR_Result with matching moves and the same outcome. |
| M13 | Two-board BLE: peer disconnect returns to Start | During a multiplayer round, power off or move the peer device out of BLE range. Verify the local device shows SCR_Disconnected/SCR_Start with Retry. |
| M14 | Two-board BLE: simultaneous Host conflict resolution | On both devices tap Host Game at nearly the same time. Verify one device remains Host (lower public MAC) and the other becomes Joiner, and both reach SCR_Gameplay. |

## 5. Code Inspection — Bug-Fix Verification

### 5.1 UI Fixes (U01–U05)

| ID | Fix | Location | Evidence |
|----|-----|----------|----------|
| U01 | Scoreboard moved to bottom center; Play Again moved up to avoid overlap | `src/ui/screens.cpp` lines 164, 201, 218 | `lv_obj_align(s.lblScore, LV_ALIGN_BOTTOM_MID, 0, -20);` on gameplay and result screens; `lv_obj_align(s.btnPlayAgain, LV_ALIGN_TOP_MID, 0, 200);` |
| U02 | All text rendered white | `src/ui/theme.cpp` | `text_secondary()` returns `lv_color_white()`; all label/button styles use `text_primary()` / `text_secondary()` |
| U03 | Timeout-dialog buttons reduced width, placed side-by-side | `src/ui/screens.cpp` lines 142–148 | Retry/Solo buttons sized 85×45 and aligned `LV_ALIGN_BOTTOM_LEFT` / `LV_ALIGN_BOTTOM_RIGHT` |
| U04 | Host-wait progress bar blue fill on black background | `src/ui/screens.cpp` lines 102–106 | `LV_PART_MAIN` bg black; `LV_PART_INDICATOR` bg `#0066FF` |
| U05 | Result screen scoreboard displays same values as gameplay | `src/ui/ui.cpp` lines 30–32, 261–262, 305–316 | Cached `s_score_*` updated by `ui_set_score()` and applied to result screen in `ui_show_screen_result()` |

### 5.2 BLE Fix (B01)

| Requirement | Location | Evidence |
|-------------|----------|----------|
| Accept Host advertisement by service UUID **or** manufacturer-data beacon signature | `src/state_machine/ble_service.cpp` lines 62–93 | `has_service_uuid` or `has_presence_beacon`; beacon signature checks company ID `0x00FF` and 12-byte length |
| Validate 4-char device ID is hex | `src/state_machine/ble_service.cpp` lines 74–88 | Loop checks characters are `0-9`, `A-F`, or `a-f` |
| Extract public MAC from advertisement address if public, else from manufacturer data | `src/state_machine/ble_service.cpp` lines 98–113 | `if (addr.getType() == BLE_ADDR_PUBLIC)` copies `addr.getNative()`; else copies `beacon_mac` from manufacturer data |
| Preserve address type for connection | `src/state_machine/ble_service.cpp` lines 116–118, 490–493; `src/state_machine/ble_service.h` line 192 | `peer_addr_type_` stored in `on_host_found()` and used in `NimBLEAddress(addr, peer_addr_type_)` |

## 6. Flash and RAM Usage Metrics

### 6.1 Physical-Hardware Environment (`esp32-2432s028r_cyd2usb`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 76,148 bytes | 327,680 bytes | 23.2% |
| Flash | 953,033 bytes | 1,310,720 bytes | 72.7% |

**Build Result:** SUCCESS  
**Partition Scheme:** `default.csv` (1.3 MB application partition)  
**Partition-Fit Check:** PASS — firmware fits within the 1,310,720-byte application partition.

### 6.2 Wokwi Simulation Environment (`esp32-2432s028r_cyd2usb_wokwi`)

| Metric | Used | Total | Percentage |
|--------|------|-------|------------|
| RAM | 68,760 bytes | 327,680 bytes | 21.0% |
| Flash | 865,605 bytes | 1,310,720 bytes | 66.0% |

**Build Result:** SUCCESS  
**Note:** The Wokwi build is smaller because NimBLE initialization and the BLE radio task are compiled out via `-D WOKWI_SIMULATION`.

## 7. Known Limitations and NO-GO Findings

1. **NimBLE unsupported in Wokwi.** Wokwi's ESP32 emulator does not emulate NimBLE reliably. `main.cpp` skips `ble_service().init()` under `WOKWI_SIMULATION`, so any path that starts BLE advertising (HOST mode) transitions to `Error` in simulation. Consequently, HOST/timeout/BLE-driven Wokwi tests (W02, W07–W09) are skipped and must be validated on physical hardware.
2. **Touch controller not emulated.** Wokwi CLI reports `unknown-part-type` for `wokwi-xpt2046`; all touch-driven tests are manual/hardware-only.
3. **No physical CYD2USB board connected.** Target tests (T01–T09) and manual touch/BLE tests cannot be executed in this run.
4. **`pio test` not configured.** The project has no `test/` directory for PlatformIO unit tests; host verification is performed by the Python harness in `tests/host/`.

**NO-GO finding:** None for the automatable scope. The firmware builds for both environments and all 73 host tests pass.

## 8. Quality Gate Assessment

| Gate | Requirement | Verdict | Evidence |
|------|-------------|---------|----------|
| G15.1 | Every automated test in `test_plan.json` passes. Zero test failures. Zero collection errors. | **PASS** | All 73 `host_assert` tests pass. Wokwi tests that do not depend on NimBLE (W01, W03–W06) pass. W02 and W07–W09 are skipped with documented Wokwi NimBLE limitation. Target tests are skipped due to absent hardware. The project's host harness is a Python script, so `pio test` is not applicable. |
| G15.2 | Every test verifies a behavioral outcome, not merely widget existence. | **PASS** | Host tests verify state transitions and action invocation; Wokwi/target tests verify serial markers and screen rendering; manual tests verify screen changes and state logs. |
| G15.3 | Manual verification procedures are documented for any test that cannot be fully automated. | **PASS** | §4 lists every touch/BLE/hardware-only test with its `manual_verification` procedure from `test_plan.json`. |
| G15.4 | Flash and RAM usage metrics are captured and reported. | **PASS** | §6 reports RAM/Flash for both physical and Wokwi builds, including partition-fit check. |
| G15.5 | Wokwi trace summary is captured for simulator tests. | **PASS** | §2.1 summarizes the live serial traces from the Wokwi smoke and clean SOLO-path runs. |

## 9. Overall QA Verdict

**PASS — Stage 15 QA objectives met; Wokwi BLE paths and hardware-only tests documented as out-of-scope for automation**

The v0.2.1 firmware builds successfully for both physical and Wokwi environments. All 73 host state-machine tests pass. The serial state instrumentation (`STATE:`, `MODE:`, `SCORE:`) is verified in Wokwi on the SOLO path. The UI fixes (U01–U05) and the BLE B01 fix are present in the source code and consistent with `docs/BugReport_CYD_RPS_v0.2.0.md`. HOST-mode and BLE-related tests, along with all touch-driven and two-board BLE tests, are documented as manual/hardware-only procedures.
