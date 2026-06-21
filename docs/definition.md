# CYD_RPS — Rock-Paper-Scissors with BLE Peer or Random Opponent

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Revision Type:** minor_revision  
**Board Version:** CYD2USB v3 (ESP32-2432S028R)  
**Target Environment:** Physical CYD2USB v3 (primary) + Wokwi simulation (non-default)  
**Simulator:** Wokwi (required non-default environment)

---

## 1. Application Overview

CYD_RPS v0.2.0 is a two-player Rock-Paper-Scissors game for ESP32-2432S028R "Cheap Yellow Display" (CYD2USB v3) boards. The v0.2.0 revision replaces the automatic public-MAC role negotiation used in v0.1.x with an explicit, user-selected **Host** model. The first user who taps **Host Game** makes that device the BLE Peripheral (Host); the peer device becomes the BLE Central (Join) and connects automatically. If no peer joins the Host before a timeout expires, a modal dialog offers **Retry** or **Solo**. A single-player mode against a uniform random-number-generator (RNG) opponent is also available by tapping **Solo** from the start screen. After each round, a session scoreboard displays cumulative wins, losses, and draws. A global home icon in the upper-right corner restarts the device from any screen. A minimal debug serial command parser is also provided for hardware testing without touch.

### Goals

- Boot on CYD2USB v3 and in Wokwi without fatal errors.
- Allow a user to explicitly designate a device as the BLE Host by tapping **Host Game**.
- Use BLE presence beacons for device discovery and a custom GATT connection for move exchange.
- Provide a one-tap **Solo** flow that skips peer discovery and plays against an RNG opponent.
- Show a Host-timeout dialog with **Retry** and **Solo** choices instead of a silent single-player fallback.
- Accept a local Rock / Paper / Scissors choice via touch buttons or serial commands.
- In multiplayer mode, transmit the local move to the peer and receive the peer's move.
- In single-player mode, generate the opponent move with a uniform RNG after the local move is chosen.
- Evaluate the round per standard RPS rules and display both moves and the outcome.
- Maintain a RAM-only session scoreboard across rounds.
- Support a repeatable new-round flow and a global firmware reset without requiring a power cycle.
- Support hardware testing of common transitions through a debug serial command parser.

### Scope

- Two-device BLE multiplayer with explicit Host selection.
- Single-player mode against a uniform RNG opponent.
- Session scoreboard (wins/losses/draws), RAM-only, reset on reboot or firmware reset.
- No persistent storage (no NVS, SPIFFS, or SD card usage for game state).
- No audio, no LED feedback, minimal animation (screen transitions, progress bar, spinner optional).
- No time limit for choosing a move.

### BLE Topology

#### Presence Beacon

- Both devices advertise a manufacturer-specific data payload while on the start screen and before a role is resolved.
- The payload contains:
  - A short device ID (the last two bytes of the public MAC rendered as four hex digits, e.g., `8D90`).
  - The full public MAC address of the device, so a Joiner can connect directly to a stable, known address.
- The beacon advertisement interval is `200 ms`.
- The beacon is sent only while the device is on `SCR_Start` and has not yet resolved a role.
- Manufacturer-specific data uses company identifier `0xFF` with a custom 10-byte payload (4-character device ID + 6-byte public MAC).

#### Host Mode

- Triggered by tapping **Host Game** on `SCR_Start` or by the serial `HOST` command.
- The device performs the following sequence:
  1. Stops the presence beacon and scanning.
  2. Sets its own address type to public (`BLE_OWN_ADDR_PUBLIC`) so it advertises and accepts connections on its public MAC.
  3. Creates the RPS GATT service and Move characteristic if not already created.
  4. Starts advertising the RPS service UUID on the public MAC.
  5. Starts the Host connection timeout timer (default `15 s`).
- Host advertising interval is `100 ms`.
- Host advertising is restarted after stopping scan with a `20 ms` guard delay to enter a clean peripheral-only connectable state.

#### Join Mode

- Triggered automatically when a start-screen scan detects a Host advertisement.
- The device performs the following sequence:
  1. Stops its own presence beacon and scanning.
  2. Extracts the Host's public MAC from the presence beacon.
  3. Connects to the Host public MAC using `BLE_ADDR_PUBLIC` directly, without reconstructing the address from `getNative()`.
  4. Discovers the RPS service and Move characteristic.
  5. Enables notifications.
- Join connection attempts use a `5 s` connect timeout.
- Join performs up to `4` connection attempts with `2 s` discovery windows between attempts.
- Join waits `50 ms` after `stop_advertising()` before each `connect()` attempt.

#### Simultaneous Host Conflict Resolution

If both users tap **Host Game** at nearly the same time:

1. Each device continues scanning while advertising as Host.
2. If a device sees a peer-Host advertisement, it immediately cancels its own Host attempt and becomes Join.
3. As a safety net, if both devices end up advertising as Host, the device with the lower BLE MAC address remains Host and the other becomes Join.

#### Connection Timeout

- Host timeout default: `15 seconds` (configurable constant).
- On timeout, a modal dialog appears over `SCR_HostWait`:
  - Title: `"No peer joined"`
  - Buttons: **Retry** (restart Host advertising and timer), **Solo** (enter single-player mode).
- Tapping **Cancel** on the hosting screen returns to `SCR_Start`.

---

## 2. UI / Display Layout

### Screen Properties

- **Resolution:** 240 × 320 portrait (USB connector at the bottom).
- **Background:** black (`#000000`).
- **Primary text:** white (`#FFFFFF`).
- **Accent color:** dark gray (`#404040`). Used for active/pressed button states and subtle highlights.
- **Outcome colors:** green for `WIN`, red for `LOSE`, white for `DRAW`.
- **Font family:** Montserrat.
- **Title font size:** 24 px.
- **Body/status font size:** 14 px.
- **Button label font size:** 16 px.
- All screens are full-screen LVGL objects created with `lv_obj_create(NULL)` and loaded with `lv_scr_load()`.

### Global Home Button

Every screen includes a small home icon button in the upper-right corner:

| Control | Type | Position | Behavior |
|---------|------|----------|----------|
| `btnHome` | `lv_btn` | Top-right, x ≈ 200 px, y ≈ 10 px, 30 × 30 px | Tapping calls `ESP.restart()` |
| `lblHomeIcon` | `lv_label` inside button | Centered | Unicode home icon `⌂` |

### SCR_Start — Start Screen

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 40 px | `"CYD RPS"` |
| `lblDeviceId` | `lv_label` | Below title, y ≈ 80 px | `"Device: 8D90"` (last 4 hex digits of MAC) |
| `lblStatus` | `lv_label` | y ≈ 120 px | `"Searching for host..."` if a Host is nearby; otherwise `"Ready"` |
| `btnHostGame` | `lv_btn` | Center, y ≈ 170 px, 180 × 60 px | `"Host Game"` — become Host |
| `btnSolo` | `lv_btn` | Below Host, y ≈ 245 px, 180 × 50 px | `"Solo"` — single-player mode |
| `btnHome` | `lv_btn` | Top-right, 30 × 30 px | Reset firmware |

When a Join device detects a Host beacon, the status label changes to `"Host found — joining..."` and the buttons are disabled until the connection resolves or fails.

### SCR_HostWait — Hosting / Waiting for Peer

Shown after tapping **Host Game**.

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 40 px | `"Hosting Game"` |
| `lblDeviceId` | `lv_label` | y ≈ 80 px | `"Device: 8D90"` |
| `lblStatus` | `lv_label` | y ≈ 120 px | `"Waiting for peer to join..."` |
| `barProgress` | `lv_bar` | y ≈ 160 px, 180 × 20 px | Timeout progress (100% → 0%) |
| `btnCancel` | `lv_btn` | Bottom-center, y ≈ 240 px, 120 × 50 px | `"Cancel"` — return to start screen |
| `btnHome` | `lv_btn` | Top-right, 30 × 30 px | Reset firmware |

On timeout, a modal dialog appears over this screen with **Retry** and **Solo**.

### SCR_Gameplay — Move Selection

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 30 px | `"Choose!"` |
| `lblScore` | `lv_label` | Top-left, x ≈ 10 px, y ≈ 30 px | `"W:0 L:0 D:0"` |
| `btnHome` | home button | Top-right | Reset firmware |
| `lblStatus` | `lv_label` | y ≈ 70 px | `"Waiting for your move"` |
| `btnRock` | `lv_btn` | y ≈ 110 px, 180 × 50 px | `"Rock"` |
| `btnPaper` | `lv_btn` | y ≈ 170 px, 180 × 50 px | `"Paper"` |
| `btnScissors` | `lv_btn` | y ≈ 230 px, 180 × 50 px | `"Scissors"` |

Move buttons use a subtle white border on black; the active/pressed state uses the accent color. Buttons are disabled after a move is chosen and re-enabled when a new round starts.

### SCR_Result — Round Result

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 30 px | `"Result"` |
| `lblScore` | `lv_label` | Top-left, x ≈ 10 px, y ≈ 30 px | `"W:1 L:0 D:0"` |
| `btnHome` | home button | Top-right | Reset firmware |
| `lblLocalMove` | `lv_label` | y ≈ 80 px | `"You: Rock"` |
| `lblPeerMove` | `lv_label` | y ≈ 110 px | `"Peer: Scissors"` |
| `lblOutcome` | `lv_label` | Center, y ≈ 160 px | `"WIN"`, `"LOSE"`, or `"DRAW"`; green/red/white |
| `btnPlayAgain` | `lv_btn` | Bottom-center, y ≈ 250 px, 160 × 55 px | `"Play Again"` |

### SCR_Error — Fatal/Retryable Error

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblErrorTitle` | `lv_label` | Top-center, y ≈ 40 px | `"Error"` |
| `lblErrorMsg` | `lv_label` | Center | Error description |
| `btnRetry` | `lv_btn` | Bottom-center, y ≈ 220 px, 120 × 50 px | `"Retry"` — return to start |
| `btnHome` | home button | Top-right | Reset firmware |

### Visual States Summary

- **Boot:** black screen, no UI visible.
- **Start:** title, local device ID, status, **Host Game** button, **Solo** button, home icon.
- **Hosting:** title, device ID, progress bar, **Cancel** button, home icon.
- **Hosting timeout dialog:** modal overlay with **Retry** and **Solo** buttons.
- **Joining:** start screen shows `"Host found — joining..."` briefly, then transitions to gameplay.
- **Gameplay:** score label, status, three move buttons, home icon.
- **Result:** score label, local/peer moves, outcome, **Play Again** button, home icon.
- **Error:** error title/message, **Retry** button, home icon.

---

## 3. User Actions

| ID | Action | Input | Precondition | System Response | API Function(s) |
|----|--------|-------|--------------|-----------------|-----------------|
| UA-01 | Power on / boot | Device reset | None | Initialize serial, TFT, touch, BLE, LVGL, and UI; start presence beacon and scan; show `SCR_Start` | `app_init()` |
| UA-02 | Host a game | Touch release on `btnHostGame` | Screen `SCR_Start` | Stop beacon/scan, start RPS service advertisement on public MAC, show `SCR_HostWait`, start 15 s timeout | `on_host_game_button_clicked()` → `ble_on_host_selected()` → `ui_show_screen_host_wait()` |
| UA-03 | Play solo from start | Touch release on `btnSolo` | Screen `SCR_Start` | Stop BLE scan/advertise, set single-player mode, show `SCR_Gameplay` | `on_solo_button_clicked()` → `game_set_mode(MODE_SINGLE_PLAYER)` → `ui_show_screen_gameplay()` |
| UA-04 | Peer auto-joins | Host advertisement detected while on `SCR_Start` | Screen `SCR_Start`, no local Host selection yet | Stop beacon/scan, connect to Host public MAC, show `SCR_Gameplay` on success | `ble_on_join_detected(host_public_mac)` → `ble_connect_to_host(host_public_mac)` → `ui_show_screen_gameplay()` |
| UA-05 | Cancel hosting | Touch release on `btnCancel` on `SCR_HostWait` | Screen `SCR_HostWait` and not yet connected | Stop advertising, restart presence beacon/scan, return to `SCR_Start` | `on_cancel_host_button_clicked()` → `ble_stop_host_advertising()` → `ble_start_presence_beacon()` → `ui_show_screen_start()` |
| UA-06 | Host timeout | 15 s timer expires without peer | Screen `SCR_HostWait` | Show Retry/Solo dialog | `ble_on_host_timeout()` → `ui_show_host_timeout_dialog()` |
| UA-07 | Retry hosting | Tap **Retry** in timeout dialog | Timeout dialog visible | Restart Host advertising and timeout, hide dialog | `on_host_retry_button_clicked()` → `ui_hide_host_timeout_dialog()` → `ble_restart_host_advertising()` |
| UA-08 | Timeout → solo | Tap **Solo** in timeout dialog | Timeout dialog visible | Enter single-player mode, hide dialog, show `SCR_Gameplay` | `on_host_solo_button_clicked()` → `ui_hide_host_timeout_dialog()` → `game_set_mode(MODE_SINGLE_PLAYER)` → `ui_show_screen_gameplay()` |
| UA-09 | Choose Rock / Paper / Scissors | Touch release on move button | Screen `SCR_Gameplay`, no local move chosen | Record local move, send/receive over BLE or generate opponent move, disable move buttons, evaluate and show result when both moves known | `on_rock_button_clicked()` / `on_paper_button_clicked()` / `on_scissors_button_clicked()` → `game_set_local_move(move)` → `ble_send_move(move)` (multiplayer) / `game_set_opponent_random()` (single-player) → `game_evaluate_round()` → `ui_set_move_buttons_enabled(false)` → `ui_show_screen_result()` |
| UA-10 | Receive peer move | BLE notification received | Multiplayer, local move already sent | Store peer move, evaluate round, switch to `SCR_Result` | `ble_on_move_received(uint8_t move)` → `game_set_peer_move(move)` → `game_evaluate_round()` → `ui_show_screen_result()` |
| UA-11 | Peer move arrives first | BLE notification received | Multiplayer, no local move chosen yet | Store peer move, update status to `"Peer chose — your turn"` | `ble_on_move_received(uint8_t move)` → `game_set_peer_move(move)` → `ui_set_status("Peer chose — your turn")` |
| UA-12 | Serial host command | Serial line `HOST` | Screen `SCR_Start` | Same as UA-02: stop beacon/scan, start Host advertising, show `SCR_HostWait`, start timeout | `serial_process_input()` → posts `EV_HOST_GAME` → `ble_on_host_selected()` → `ui_show_screen_host_wait()` |
| UA-13 | Serial solo command | Serial line `SOLO` | Screen `SCR_Start` or timeout dialog | Same as UA-03: stop BLE scan/advertise, set single-player mode, show `SCR_Gameplay` | `serial_process_input()` → posts `EV_SOLO` → `game_set_mode(MODE_SINGLE_PLAYER)` → `ui_show_screen_gameplay()` |
| UA-14 | Serial rock command | Serial line `ROCK` | Screen `SCR_Gameplay`, no local move chosen | Same as UA-09 for Rock | `serial_process_input()` → posts `EV_MOVE_ROCK` → `game_set_local_move(MOVE_ROCK)` → … |
| UA-15 | Serial paper command | Serial line `PAPER` | Screen `SCR_Gameplay`, no local move chosen | Same as UA-09 for Paper | `serial_process_input()` → posts `EV_MOVE_PAPER` → `game_set_local_move(MOVE_PAPER)` → … |
| UA-16 | Serial scissors command | Serial line `SCISSORS` | Screen `SCR_Gameplay`, no local move chosen | Same as UA-09 for Scissors | `serial_process_input()` → posts `EV_MOVE_SCISSORS` → `game_set_local_move(MOVE_SCISSORS)` → … |
| UA-17 | Serial again command | Serial line `AGAIN` | Screen `SCR_Result` | Same as UA-19: clear local/peer moves, re-enable move buttons, switch to `SCR_Gameplay` | `serial_process_input()` → posts `EV_PLAY_AGAIN` → `game_reset_round()` → `ui_set_move_buttons_enabled(true)` → `ui_show_screen_gameplay()` |
| UA-18 | Serial reset/home command | Serial line `RESET` or `HOME` | Any screen | Same as UA-20: call `ESP.restart()` | `serial_process_input()` → posts `EV_RESET` → `on_home_button_clicked()` → `ESP.restart()` |
| UA-19 | Start new round | Touch release on `btnPlayAgain` | Screen `SCR_Result` | Clear local/peer moves, re-enable move buttons, switch to `SCR_Gameplay` | `on_play_again_button_clicked()` → `game_reset_round()` → `ui_set_move_buttons_enabled(true)` → `ui_show_screen_gameplay()` |
| UA-20 | Firmware reset | Tap home icon on any screen | Any screen | Call `ESP.restart()` | `on_home_button_clicked()` → `ESP.restart()` |
| UA-21 | Peer disconnects | BLE disconnect event | Any connected multiplayer state | Update status, return to `SCR_Start` | `ble_on_disconnected()` → `ui_show_screen_start()` |
| UA-22 | Retry from error | Touch release on `btnRetry` | Screen `SCR_Error` | Disconnect, reset game state, return to `SCR_Start` | `on_retry_button_clicked()` → `ble_disconnect()` → `game_reset_all()` → `ui_show_screen_start()` |

---

## 4. Data Model / Persistence

### Runtime Data

| Name | Type | Scope | Description |
|------|------|-------|-------------|
| `device_id_str` | `char[5]` | Global/static | Four-hex-digit local device ID derived from the public MAC (e.g., `"8D90"`). |
| `peer_device_id_str` | `char[5]` | Global/static | Device ID of the connected peer (multiplayer only). |
| `session_score_t` | `struct { uint16_t wins; uint16_t losses; uint16_t draws; }` | Global/static | Cumulative round results for the current power-on session. |
| `role_t` | `enum { ROLE_NONE, ROLE_HOST, ROLE_JOIN }` | Global/static | Current BLE role. `ROLE_NEGOTIATING` is removed because roles are now user-selected or auto-joined. |
| `game_mode_t` | `enum { MODE_SINGLE_PLAYER, MODE_MULTI_PLAYER }` | Global/static | Current opponent source. |
| `move_t` | `enum { MOVE_NONE=0, MOVE_ROCK=1, MOVE_PAPER=2, MOVE_SCISSORS=3 }` | Global/static | Player move. Used for both local and peer/opponent moves. |
| `outcome_t` | `enum { OUTCOME_NONE, OUTCOME_WIN, OUTCOME_LOSE, OUTCOME_DRAW }` | Global/static | Result of the current/last round. |
| `game_state_t` | `enum { STATE_BOOT, STATE_START, STATE_HOST_WAIT, STATE_GAMEPLAY, STATE_RESULT, STATE_ERROR }` | Global/static | High-level UI/game state. `STATE_PEER_SEARCH` and `STATE_CONNECTING` are removed; `STATE_HOST_WAIT` is added. |
| `ble_connected` | `bool` | Global/static | True when a multiplayer BLE link is established. |
| `ui_ready` | `bool` | Global/static | True after LVGL and all screens are created successfully. |
| `event_queue_mutex` | `SemaphoreHandle_t` | Global/static | FreeRTOS mutex protecting the state-machine event queue. |
| `radio_command_queue` | `QueueHandle_t` | Global/static | FreeRTOS queue used to post radio commands to the dedicated BLE radio task. |
| `serial_command_buffer` | `char[32]` | Global/static | Ring/line buffer accumulating bytes received on `Serial`. |
| `serial_command_ready` | `bool` | Global/static | Set to `true` when a complete command line is available in `serial_command_buffer`. |

### Score Update Rules

- After each round, increment the appropriate counter based on the local outcome:
  - `OUTCOME_WIN`  → `wins++`
  - `OUTCOME_LOSE` → `losses++`
  - `OUTCOME_DRAW` → `draws++`
- Scores reset to zero on boot or firmware reset.
- No persistence across reboots.

### BLE Service / Characteristic Layout

A single custom GATT service and characteristic are used for multiplayer move exchange. The service is not used in single-player mode.

| Attribute | UUID | Properties | Purpose |
|-----------|------|------------|---------|
| RPS Service | `a07498ca-ad5b-474e-940d-263601a52152` | — | Custom service UUID. |
| Move Characteristic | `a07498ca-ad5b-474e-940d-263601a52153` | Read, Write, Notify | Carries a single-byte move value (`MOVE_*` encoding). The local side writes its move; the remote side receives it via notification. |

Protocol flow:

1. Both devices initialize NimBLE, create the RPS service and characteristic, and start the GATT server during `ble_init()` before any GAP scan/advertise/connection procedure is active.
2. On boot, both devices send presence beacons and scan for peer Host advertisements.
3. Tapping **Host Game** (or serial `HOST`) stops the beacon and scan, sets public address ownership, and starts advertising the RPS service UUID on the public MAC.
4. A peer still on `SCR_Start` detects the Host advertisement, stops its own beacon/scan, and connects directly to the Host public MAC.
5. After connection, the link is symmetric: both sides can write their move to the same Move characteristic and receive the peer's move via notifications.
6. In single-player mode, no BLE transmission occurs; the opponent move is generated locally after the local move is recorded.

### Persistence

- **No persistence in v0.2.0.** Round history, scores, and peer information are stored only in RAM and reset to zero on every reboot or firmware reset.
- **Default storage:** none. No SD card, SPIFFS, or NVS usage in v0.2.0.
- If a future revision adds score persistence, it will use ESP32 NVS (no SD dependency).

---

## 5. API Function List

### Application Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `app_init` | `void app_init(void)` | `setup()` | Initialize serial, TFT, touch, BLE stack, LVGL, and UI. Logs `"CYD_RPS setup done"` on success. |
| `app_run` | `void app_run(void)` | `loop()` | Run LVGL timer handler, poll BLE events, dispatch state-machine events, call `serial_process_input()` and `serial_command_dispatch()`. Must not block. |
| `app_on_error` | `void app_on_error(const char* msg)` | Init or runtime failure | Transition to `SCR_Error` and log the error. |

### UI Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `ui_create` | `void ui_create(void)` | `app_init()` success | Create all screens and widgets and bind event handlers. |
| `ui_show_screen_start` | `void ui_show_screen_start(void)` | Boot complete, cancel, retry, peer disconnect, or error retry | Load `SCR_Start`. |
| `ui_show_screen_host_wait` | `void ui_show_screen_host_wait(void)` | Host Game button pressed | Load the hosting/waiting screen. |
| `ui_show_host_timeout_dialog` | `void ui_show_host_timeout_dialog(void)` | Host timeout | Show the Retry/Solo dialog over `SCR_HostWait`. |
| `ui_hide_host_timeout_dialog` | `void ui_hide_host_timeout_dialog(void)` | Retry or Solo in timeout dialog | Dismiss the Retry/Solo dialog. |
| `ui_show_screen_gameplay` | `void ui_show_screen_gameplay(void)` | Connected, new round, or single-player fallback | Load `SCR_Gameplay`, enable move buttons, clear status. |
| `ui_show_screen_result` | `void ui_show_screen_result(move_t local, move_t peer, outcome_t outcome)` | Round evaluated | Load `SCR_Result` with move and outcome text. |
| `ui_show_screen_error` | `void ui_show_screen_error(const char* msg)` | Error event | Load `SCR_Error` with message. |
| `ui_set_status` | `void ui_set_status(const char* text)` | Connection/move events | Update the current screen's status label. |
| `ui_set_device_id` | `void ui_set_device_id(const char* id)` | Boot / role change | Update the device ID label on `SCR_Start` and `SCR_HostWait`. |
| `ui_set_peer_device_id` | `void ui_set_peer_device_id(const char* id)` | Peer connected | Show the peer device ID in status text (optional). |
| `ui_set_score` | `void ui_set_score(uint16_t wins, uint16_t losses, uint16_t draws)` | Round result | Update the score label on gameplay/result screens. |
| `ui_set_host_wait_progress` | `void ui_set_host_wait_progress(uint8_t percent)` | Host timeout timer | Set `barProgress` value (100 → 0). |
| `ui_set_move_buttons_enabled` | `void ui_set_move_buttons_enabled(bool enabled)` | Move chosen / new round | Enable or disable `btnRock`, `btnPaper`, `btnScissors`. |
| `ui_add_home_button` | `void ui_add_home_button(lv_obj_t* screen)` | Screen creation | Helper to add the home reset button to any screen. |

### BLE Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `ble_init` | `bool ble_init(void)` | `app_init()` | Initialize the ESP32 BLE stack (NimBLE-Arduino), create the RPS service/characteristic, start the GATT server, and spawn the dedicated radio task. Returns `true` on success. |
| `ble_start_presence_beacon` | `void ble_start_presence_beacon(void)` | UA-01, UA-05 | Start sending the presence beacon and scanning for Host advertisements while on `SCR_Start`. |
| `ble_stop_presence_beacon` | `void ble_stop_presence_beacon(void)` | UA-02, UA-04, UA-05 | Stop sending the presence beacon and scanning. |
| `ble_on_host_selected` | `void ble_on_host_selected(void)` | UA-02 / UA-12 | Called when user taps **Host Game** or serial `HOST` is received. Posts the `BECOME_HOST` radio command. |
| `ble_stop_host_advertising` | `void ble_stop_host_advertising(void)` | UA-05 | Stop the Host RPS service advertisement and cancel the Host timeout timer. |
| `ble_on_join_detected` | `void ble_on_join_detected(const uint8_t host_public_mac[6])` | UA-04 | Called when a Host beacon is seen and this device should connect. Posts the `CONNECT_TO_HOST` radio command with the public MAC. |
| `ble_on_peer_found` | `void ble_on_peer_found(const uint8_t peer_public_mac[6])` | BLE scan/advertisement event | Callback invoked when a peer RPS Host advertisement is discovered. |
| `ble_connect_to_host` | `bool ble_connect_to_host(const uint8_t host_public_mac[6])` | Radio task | Connect to the Host public MAC with `BLE_ADDR_PUBLIC`, discover the RPS service and characteristic, and enable notifications. Returns `true` on success. |
| `ble_disconnect` | `void ble_disconnect(void)` | Retry / peer disconnect | Drop the current BLE connection. |
| `ble_on_connected` | `void ble_on_connected(void)` | BLE connection event | Callback invoked when the BLE link is up and the GATT service is ready. |
| `ble_on_host_timeout` | `void ble_on_host_timeout(void)` | 15 s timer | Callback invoked when no peer joins the Host session. |
| `ble_restart_host_advertising` | `void ble_restart_host_advertising(void)` | UA-07 | Restart Host advertising and the 15 s timeout timer. |
| `ble_send_move` | `bool ble_send_move(uint8_t move)` | UA-09 / UA-14..16 (multiplayer) | Transmit the local move byte to the peer. Returns `true` on success. |
| `ble_on_move_received` | `void ble_on_move_received(uint8_t move)` | BLE notification/write | Callback invoked when the peer's move arrives. |
| `ble_on_disconnected` | `void ble_on_disconnected(void)` | BLE disconnect event | Callback invoked when the peer disconnects. |
| `ble_is_connected` | `bool ble_is_connected(void)` | Any state query | Returns `ble_connected`. |
| `radio_task_loop` | `void radio_task_loop(void* param)` | `ble_init()` | FreeRTOS task that owns all NimBLE operations (`START_PRESENCE`, `STOP_PRESENCE`, `BECOME_HOST`, `CONNECT_TO_HOST`, `DISCONNECT`). |

### Game Logic Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `game_set_mode` | `void game_set_mode(game_mode_t mode)` | UA-03 / UA-04 / UA-08 / UA-13 | Set the current opponent source mode. |
| `game_set_local_move` | `void game_set_local_move(move_t move)` | UA-09 / UA-14..16 | Store the local move and check whether the round can be evaluated. |
| `game_set_peer_move` | `void game_set_peer_move(uint8_t move)` | UA-10/11 | Store and validate the peer move. |
| `game_set_opponent_random` | `void game_set_opponent_random(void)` | UA-03/UA-09/UA-14..16 single-player branch | Generate a uniform random opponent move (`MOVE_ROCK`, `MOVE_PAPER`, or `MOVE_SCISSORS`) and store it as the peer move. |
| `game_evaluate_round` | `outcome_t game_evaluate_round(void)` | Both moves known | Apply standard RPS rules, update the session score, and return `OUTCOME_WIN`, `OUTCOME_LOSE`, or `OUTCOME_DRAW`. |
| `game_reset_round` | `void game_reset_round(void)` | UA-04 / UA-17 / UA-19 | Clear local and peer moves for the next round. |
| `game_reset_all` | `void game_reset_all(void)` | UA-22 | Reset mode, role, connection, and game state to post-boot values. |
| `move_to_string` | `const char* move_to_string(move_t move)` | UI/serial logging | Map `MOVE_ROCK` → `"Rock"`, `MOVE_PAPER` → `"Paper"`, `MOVE_SCISSORS` → `"Scissors"`. |
| `outcome_to_string` | `const char* outcome_to_string(outcome_t outcome)` | UI/serial logging | Map outcome enum to `"WIN"`, `"LOSE"`, `"DRAW"`. |

### Serial / Debug Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `serial_process_input` | `void serial_process_input(void)` | `app_run()` each loop | Read bytes from `Serial`, accumulate into `serial_command_buffer`, and on a newline set `serial_command_ready`. |
| `serial_command_dispatch` | `void serial_command_dispatch(void)` | `app_run()` when `serial_command_ready` is true | Parse the buffered command (`HOST`, `SOLO`, `ROCK`, `PAPER`, `SCISSORS`, `AGAIN`, `RESET`, `HOME`) and post the equivalent state-machine event. |

### Event Handlers (LVGL callbacks)

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `on_host_game_button_clicked` | `static void on_host_game_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnHostGame` | Post `EV_HOST_GAME` event. |
| `on_solo_button_clicked` | `static void on_solo_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnSolo` | Post `EV_SOLO` event. |
| `on_cancel_host_button_clicked` | `static void on_cancel_host_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnCancel` on `SCR_HostWait` | Post `EV_CANCEL_HOST` event. |
| `on_host_retry_button_clicked` | `static void on_host_retry_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on timeout dialog **Retry** | Post `EV_HOST_RETRY` event. |
| `on_host_solo_button_clicked` | `static void on_host_solo_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on timeout dialog **Solo** | Post `EV_HOST_SOLO` event. |
| `on_rock_button_clicked` | `static void on_rock_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnRock` | Post `EV_MOVE_ROCK` event. |
| `on_paper_button_clicked` | `static void on_paper_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnPaper` | Post `EV_MOVE_PAPER` event. |
| `on_scissors_button_clicked` | `static void on_scissors_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnScissors` | Post `EV_MOVE_SCISSORS` event. |
| `on_play_again_button_clicked` | `static void on_play_again_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnPlayAgain` | Post `EV_PLAY_AGAIN` event. |
| `on_retry_button_clicked` | `static void on_retry_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnRetry` | Post `EV_RETRY` event. |
| `on_home_button_clicked` | `static void on_home_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnHome` | Call `ESP.restart()` (or defer to main loop if required). |

### Touch Mapping Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `touch_read_cb` | `void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data)` | LVGL indev read | Read raw XPT2046 ADC, map to screen pixels, clamp to display bounds, and report pressed/released state. |
| `touch_map_adc_to_screen` | `void touch_map_adc_to_screen(uint16_t raw_x, uint16_t raw_y, int16_t* screen_x, int16_t* screen_y)` | `touch_read_cb` | Convert raw 0–4095 ADC values to 0–239 / 0–319 screen coordinates. |

---

## 6. Toolchain

| Component | Value | Notes |
|-----------|-------|-------|
| Language | C/C++ (Arduino-ESP32) | Matches CYD2USB_HelloLVGL and CYD_SmokeTest2 references. |
| Framework | Arduino | `framework = arduino` in `platformio.ini`. |
| Platform | `espressif32` | PlatformIO. |
| Board | `esp32-2432s028r_cyd2usb` (preferred) or `esp32dev` fallback | Per `skills/platformio`. |
| GUI library | LVGL 8.4.0 | `lvgl@8.4.0` (stable CYD2USB reference version). |
| Display driver | TFT_eSPI 2.5.43 | `bodmer/TFT_eSPI@^2.5.43`. |
| Touch driver | XPT2046_Touchscreen | `paulstoffregen/XPT2046_Touchscreen`. |
| BLE library | NimBLE-Arduino | `h2zero/NimBLE-Arduino@^1.4.0` for smaller flash footprint. |
| Build tool | PlatformIO `pio run` | Default environment `esp32-2432s028r_cyd2usb` produces `.pio/build/esp32-2432s028r_cyd2usb/firmware.bin`. |
| Static analysis | `pio check` | Must report no critical/high defects before release. |
| Simulator | Wokwi CLI | `wokwi-cli` with `wokwi/diagram.json` and `wokwi/scenario.yaml`. Touch-driven tests are classified as `manual`/`hardware` per LL-032. BLE peer-discovery, connection, and multiplayer transitions are classified as `untestable_transitions` because Wokwi does not support NimBLE initialization. |
| Serial monitor | 115200 baud | Standard CYD2USB rate; also used by the debug serial command parser. |
| Debug serial parser | Built-in, no extra dependency | Enabled for all build environments. Uses Arduino `Serial` to accept `HOST`, `SOLO`, `ROCK`, `PAPER`, `SCISSORS`, `AGAIN`, `RESET`, `HOME` commands. |

### Build Flags

- `-D USER_SETUP_LOADED`
- `-include "${PROJECT_DIR}/include/User_Setup.h"`
- `-include "${PROJECT_DIR}/include/lv_conf.h"`
- `-D LV_CONF_INCLUDE_SIMPLE`
- `-D WOKWI_SIMULATION` (Wokwi environment only; skips NimBLE init to avoid emulator watchdog)

### Board Build Parameters

- `board_build.f_cpu = 240000000L`
- `board_build.f_flash = 40000000L`
- `board_build.flash_mode = dio`
- `upload_speed = 460800`

---

## 7. Hardware Profile

### Board

- **Version:** CYD2USB v3 (ESP32-2432S028R)
- **Display controller:** ST7789, 240 × 320 native, used in portrait 240 × 320
- **Touch controller:** XPT2046 resistive touchscreen
- **Wireless:** ESP32 integrated BLE radio (no external antenna or wiring)
- **Backlight:** GPIO 21

### Pin Map (from `skills/cyd_hardware` and CYD2USB reference)

| Function | GPIO | Notes |
|----------|------|-------|
| TFT_MOSI | 13 | HSPI |
| TFT_MISO | 12 | HSPI |
| TFT_SCK  | 14 | HSPI |
| TFT_CS   | 15 | HSPI |
| TFT_DC   | 2  | HSPI |
| TFT_BL   | 21 | Backlight PWM/output |
| TFT_RST  | -1 | Tied to ESP32 board reset |
| TOUCH_MOSI | 32 | VSPI |
| TOUCH_MISO | 39 | VSPI (VN) |
| TOUCH_CLK  | 25 | VSPI |
| TOUCH_CS   | 33 | VSPI |
| TOUCH_IRQ  | 36 | Polled; IRQ may be disabled by setting 255 if not used |

### Reserved Pins

- Do not use GPIO 0, 2, 4, 5, 12, 15 for user peripherals without checking boot-mode impact.
- GPIO 34–39 are input-only.

### Free / External Pin List

CYD_RPS v0.2.0 uses only onboard peripherals (display, touch, BLE radio). No external pins are required. The free pins remain available for future expansion:

- GPIO 22
- GPIO 27
- GPIO 35

### Wokwi Delta

- Wokwi uses the `wokwi-ili9341` part as a generic 240 × 320 framebuffer in place of the physical ST7789 panel.
- The `wokwi-xpt2046` part is documented in the hardware profile but is not supported by Wokwi CLI; touch-driven tests must be classified as `manual`/`hardware` per LL-032.
- Color byte order and gamma may differ from the physical board; visual correctness is validated on hardware.
- BLE pairing is not exercised in Wokwi CLI smoke tests; BLE functionality is validated on physical hardware or via host-level unit tests of the game logic.
- Single-player fallback can be exercised in Wokwi by injecting a discovery-timeout event or by asserting that the firmware enters gameplay after the timeout period.
- The debug serial command parser is available in all builds, including Wokwi, because it only requires the standard Arduino `Serial` object.

### Power / Connection Notes

- Each CYD2USB board is powered via its USB-C connector.
- No wired connection between two boards is required; communication uses the ESP32 BLE radio.
- Keep the two boards within BLE range (typically ≤ 10 m indoors) during multiplayer play.
