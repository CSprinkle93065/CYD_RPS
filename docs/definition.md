# CYD_RPS — Rock-Paper-Scissors with BLE Peer or Random Opponent

**Project:** CYD_RPS  
**Version:** 0.1.0  
**Workflow ID:** wvc_20260617_044037  
**Revision Type:** new_project  
**Board Version:** CYD2USB v3 (ESP32-2432S028R)  
**Target Environment:** Wokwi simulation + physical CYD2USB v3  
**Simulator:** Wokwi (required)

---

## 1. Application Overview

CYD_RPS is a "Rock, Paper, Scissors" game for one or two ESP32-2432S028R "Cheap Yellow Display" (CYD2USB v3) boards. In multiplayer mode, two CYD units discover each other over Bluetooth Low Energy (BLE), auto-negotiate their roles, exchange moves over a custom GATT service, and display the round result. If a second device is not discovered within a fixed timeout, the firmware falls back to single-player mode and plays against a uniform random-number-generator (RNG) opponent. The gameplay and result screens are identical in both modes; only the source of the opponent move differs.

### Goals

- Boot on CYD2USB v3 and in Wokwi without fatal errors.
- Auto-negotiate the BLE Peripheral/Central roles using a deterministic public BLE MAC address tie-breaker.
- Provide a one-tap start flow that searches for a BLE peer and falls back to single-player mode if none is found.
- Accept a local Rock / Paper / Scissors choice via touch buttons.
- In multiplayer mode, transmit the local move to the peer and receive the peer's move.
- In single-player mode, generate the opponent move with a uniform RNG after the local move is chosen.
- Evaluate the round per standard RPS rules and display both moves and the outcome.
- Support a repeatable new-round flow without requiring a reboot.

### Scope

- Two-device BLE topology when a peer is available: one Peripheral (Host) and one Central (Join).
- Role selection is fully automatic; no manual Host/Join UI exists.
- Single-player fallback activates if no peer is discovered within 10 seconds of starting a search.
- Scores are RAM-only and reset to zero on every reboot; no NVS, SPIFFS, or SD card usage for persistence.
- No audio, no LED feedback, no animation beyond label updates, a search progress bar, and screen transitions.
- No time limit for choosing a move in v0.1.0.

### BLE Topology

- Both devices start in the same peer-discovery state. Each device advertises the presence of the RPS service and scans for the peer.
- When two devices discover each other, they compare their public BLE device addresses to decide roles.
- The device with the **lower** public BLE MAC address acts as the BLE **Peripheral** (Host role) and begins advertising the RPS service.
- The device with the **higher** public BLE MAC address acts as the BLE **Central** (Join role) and connects to the Peripheral.
- After connection, the link is symmetric: both sides can write their move to the same Move characteristic and receive the peer's move via notifications.
- If no peer is found within 10 seconds, the device stops discovery and enters single-player mode; the opponent move is generated locally by an RNG.

---

## 2. UI/Display Layout

### Screen Properties

- **Resolution:** 240 × 320 portrait (USB connector at the bottom). This is the CYD2USB v3 default orientation per `skills/cyd_hardware`.
- **Background:** dark theme (`#1a1a1a` or LVGL default dark background).
- **Theme:** Montserrat font family; labels white, buttons blue/grey accent, outcome text colored by result.

### Screens and Controls

All screens are full-screen LVGL objects created with `lv_obj_create(NULL)` and loaded with `lv_scr_load()`.

#### SCR_Start — Start / Find Peer

| Control | Type | ID | Position / Size | Style | Purpose |
|---------|------|----|-----------------|-------|---------|
| `lblTitle` | `lv_label` | `lblTitle` | Top-center, y ≈ 40 px | Font 24, white | Static text `"CYD RPS"` |
| `lblSubtitle` | `lv_label` | `lblSubtitle` | Below title, y ≈ 80 px | Font 14, light grey | Static text `"Tap Play to find a peer"` |
| `btnPlay` | `lv_btn` | `btnPlay` | Center, y ≈ 150 px, 120 × 60 px | Blue accent, label `"Play"` | Start BLE peer discovery and role negotiation |
| `lblStatus` | `lv_label` | `lblStatusStart` | Bottom-center, y ≈ 250 px | Font 14, yellow | Dynamic status: `"Ready"`, `"Starting..."` |

#### SCR_PeerSearch — Searching for Peer

| Control | Type | ID | Position / Size | Style | Purpose |
|---------|------|----|-----------------|-------|---------|
| `lblTitle` | `lv_label` | `lblTitle` | Top-center, y ≈ 40 px | Font 24, white | Static text `"CYD RPS"` |
| `lblStatus` | `lv_label` | `lblStatusSearch` | y ≈ 90 px | Font 16, white | Dynamic status: `"Searching for peer..."`, `"Negotiating role..."`, `"Connecting..."` |
| `barSearch` | `lv_bar` | `barSearch` | y ≈ 140 px, 180 × 20 px | Blue fill on grey background | Search progress (0–100%) |
| `lblTimeout` | `lv_label` | `lblTimeout` | Below bar, y ≈ 170 px | Font 14, light grey | Countdown text: `"10s remaining"` |
| `btnCancel` | `lv_btn` | `btnCancel` | Bottom-center, y ≈ 240 px, 120 × 50 px | Grey, label `"Cancel"` | Abort search and return to `SCR_Start` |

#### SCR_Gameplay — Move Selection

| Control | Type | ID | Position / Size | Style | Purpose |
|---------|------|----|-----------------|-------|---------|
| `lblTitle` | `lv_label` | `lblTitle` | Top-center, y ≈ 30 px | Font 22, white | Static text `"Choose!"` |
| `lblStatus` | `lv_label` | `lblStatusGame` | y ≈ 65 px | Font 14, light grey | Dynamic status: `"Waiting for your move"`, `"Move sent — waiting for peer"`, `"Peer: <move>"` |
| `btnRock` | `lv_btn` | `btnRock` | y ≈ 110 px, 180 × 50 px | Grey, label `"✊ Rock"` | Select Rock |
| `btnPaper` | `lv_btn` | `btnPaper` | y ≈ 170 px, 180 × 50 px | Grey, label `"✋ Paper"` | Select Paper |
| `btnScissors` | `lv_btn` | `btnScissors` | y ≈ 230 px, 180 × 50 px | Grey, label `"✌ Scissors"` | Select Scissors |

Buttons are disabled after a move is chosen and re-enabled when a new round starts.

#### SCR_Result — Round Result

| Control | Type | ID | Position / Size | Style | Purpose |
|---------|------|----|-----------------|-------|---------|
| `lblTitle` | `lv_label` | `lblTitle` | Top-center, y ≈ 30 px | Font 22, white | Static text `"Result"` |
| `lblLocalMove` | `lv_label` | `lblLocalMove` | y ≈ 80 px | Font 16, white | Shows `"You: <move>"` |
| `lblPeerMove` | `lv_label` | `lblPeerMove` | y ≈ 110 px | Font 16, white | Shows `"Peer: <move>"` |
| `lblOutcome` | `lv_label` | `lblOutcome` | Center, y ≈ 160 px | Font 32, bold; green for WIN, red for LOSE, white for DRAW | Shows `"WIN"`, `"LOSE"`, or `"DRAW"` |
| `btnPlayAgain` | `lv_btn` | `btnPlayAgain` | Bottom-center, y ≈ 250 px, 160 × 55 px | Blue accent, label `"Play Again"` | Start a new round |

#### SCR_Error — Fatal/Retryable Error

| Control | Type | ID | Position / Size | Style | Purpose |
|---------|------|----|-----------------|-------|---------|
| `lblErrorTitle` | `lv_label` | `lblErrorTitle` | Top-center, y ≈ 40 px | Font 20, red | Static text `"Error"` |
| `lblErrorMsg` | `lv_label` | `lblErrorMsg` | Center | Font 14, white | Dynamic error description |
| `btnRetry` | `lv_btn` | `btnRetry` | Bottom-center, y ≈ 240 px, 120 × 50 px | Grey, label `"Retry"` | Return to start / reinitialize |

### Visual States Summary

- **Boot:** screen black, no UI visible (during `setup()` hardware init).
- **Start:** title + `Play` button.
- **PeerSearch:** status label + progress bar + timeout countdown + Cancel button.
- **Gameplay:** three enabled move buttons; disabled after local move is chosen.
- **Result:** local move, peer move, outcome, Play Again button.
- **Error:** red title, error message, Retry button.

---

## 3. User Actions

| ID | Action | Input | Precondition | System Response | API Function(s) |
|----|--------|-------|--------------|-----------------|-----------------|
| UA-01 | Power on / boot | Device reset | None | Initialize serial, TFT, touch, BLE, LVGL, and UI; log `CYD_RPS setup done` | `app_init()` |
| UA-02 | Start peer search | Touch release on `btnPlay` | Screen `SCR_Start` | Start BLE discovery with 10 s timeout, reset game state, switch to `SCR_PeerSearch` | `on_play_button_clicked()` → `ble_start_discovery()` → `ui_show_screen_peer_search()` |
| UA-03 | Cancel peer search | Touch release on `btnCancel` | Screen `SCR_PeerSearch` and not yet connected | Stop advertising/scanning, disconnect if any, return to `SCR_Start` | `on_cancel_button_clicked()` → `ble_stop_discovery()` → `ui_show_screen_start()` |
| UA-04 | Peer discovered and role resolved | BLE advertisement/scan result | Screen `SCR_PeerSearch` | Compare public BLE MAC addresses, assign Host/Join roles, update status to `"Connecting..."` | `ble_on_peer_found(peer_addr)` → `ble_resolve_role(local_addr, peer_addr)` → `ui_set_status("Connecting...")` |
| UA-05 | Peer connected | BLE connection event | Role resolved and connecting | Mark BLE connected, reset round, switch to `SCR_Gameplay` | `ble_on_connected()` → `game_set_mode(MODE_MULTI_PLAYER)` → `game_reset_round()` → `ui_show_screen_gameplay()` |
| UA-06 | Discovery timeout — single-player fallback | 10 s timer expires without peer | Screen `SCR_PeerSearch` | Stop discovery, set single-player mode, switch to `SCR_Gameplay` | `ble_on_discovery_timeout()` → `game_set_mode(MODE_SINGLE_PLAYER)` → `ui_show_screen_gameplay()` |
| UA-07 | Choose Rock | Touch release on `btnRock` | Screen `SCR_Gameplay`, no local move chosen | Record `MOVE_ROCK`; in multiplayer send to peer; in single-player generate opponent move; if both moves are known, evaluate and show result, otherwise disable buttons and update status | `on_rock_button_clicked()` → `game_set_local_move(MOVE_ROCK)` → `ble_send_move(MOVE_ROCK)` (multiplayer) / `game_set_opponent_random()` (single-player) → `game_evaluate_round()` → `ui_set_move_buttons_enabled(false)` → `ui_show_screen_result()` |
| UA-08 | Choose Paper | Touch release on `btnPaper` | Screen `SCR_Gameplay`, no local move chosen | Record `MOVE_PAPER`; in multiplayer send to peer; in single-player generate opponent move; if both moves are known, evaluate and show result, otherwise disable buttons and update status | `on_paper_button_clicked()` → `game_set_local_move(MOVE_PAPER)` → `ble_send_move(MOVE_PAPER)` (multiplayer) / `game_set_opponent_random()` (single-player) → `game_evaluate_round()` → `ui_set_move_buttons_enabled(false)` → `ui_show_screen_result()` |
| UA-09 | Choose Scissors | Touch release on `btnScissors` | Screen `SCR_Gameplay`, no local move chosen | Record `MOVE_SCISSORS`; in multiplayer send to peer; in single-player generate opponent move; if both moves are known, evaluate and show result, otherwise disable buttons and update status | `on_scissors_button_clicked()` → `game_set_local_move(MOVE_SCISSORS)` → `ble_send_move(MOVE_SCISSORS)` (multiplayer) / `game_set_opponent_random()` (single-player) → `game_evaluate_round()` → `ui_set_move_buttons_enabled(false)` → `ui_show_screen_result()` |
| UA-10 | Receive peer move | BLE notification received | Multiplayer, local move already sent | Store peer move, evaluate round, switch to `SCR_Result` | `ble_on_move_received(uint8_t move)` → `game_set_peer_move(move)` → `game_evaluate_round()` → `ui_show_screen_result()` |
| UA-11 | Peer move arrives first | BLE notification received | Multiplayer, no local move chosen yet | Store peer move, update status to `"Peer chose — your turn"` | `ble_on_move_received(uint8_t move)` → `game_set_peer_move(move)` → `ui_set_status("Peer chose — your turn")` |
| UA-12 | Start new round | Touch release on `btnPlayAgain` | Screen `SCR_Result` | Clear local/peer moves, re-enable move buttons, switch to `SCR_Gameplay` | `on_play_again_button_clicked()` → `game_reset_round()` → `ui_set_move_buttons_enabled(true)` → `ui_show_screen_gameplay()` |
| UA-13 | Retry from error | Touch release on `btnRetry` | Screen `SCR_Error` | Disconnect, reset game state, return to `SCR_Start` | `on_retry_button_clicked()` → `ble_disconnect()` → `game_reset_all()` → `ui_show_screen_start()` |
| UA-14 | Peer disconnects | BLE disconnect event | Any connected multiplayer state | Update status, return to `SCR_Start` | `ble_on_disconnected()` → `ui_show_screen_start()` |

---

## 4. Data Model / Persistence

### Runtime Data

| Name | Type | Scope | Description |
|------|------|-------|-------------|
| `role_t` | `enum { ROLE_NONE, ROLE_NEGOTIATING, ROLE_HOST, ROLE_JOIN }` | Global/static | Current BLE role during negotiation and after resolution. |
| `game_mode_t` | `enum { MODE_SINGLE_PLAYER, MODE_MULTI_PLAYER }` | Global/static | Current opponent source. Defaults to `MODE_MULTI_PLAYER` during peer search; falls back to `MODE_SINGLE_PLAYER` on discovery timeout. |
| `move_t` | `enum { MOVE_NONE=0, MOVE_ROCK=1, MOVE_PAPER=2, MOVE_SCISSORS=3 }` | Global/static | Player move. Used for both local and peer/opponent moves. |
| `outcome_t` | `enum { OUTCOME_NONE, OUTCOME_WIN, OUTCOME_LOSE, OUTCOME_DRAW }` | Global/static | Result of the current/last round. |
| `game_state_t` | `enum { STATE_BOOT, STATE_START, STATE_PEER_SEARCH, STATE_CONNECTING, STATE_GAMEPLAY, STATE_RESULT, STATE_ERROR }` | Global/static | High-level UI/game state. |
| `ble_connected` | `bool` | Global/static | True when a multiplayer BLE link is established. |
| `ui_ready` | `bool` | Global/static | True after LVGL and all screens are created successfully. |

### BLE Service / Characteristic Layout

A single custom GATT service and characteristic are used for multiplayer move exchange. The service is not used in single-player mode.

| Attribute | UUID | Properties | Purpose |
|-----------|------|------------|---------|
| RPS Service | `a07498ca-ad5b-474e-940d-263601a52152` | — | Custom service UUID selected by the Definition Author. |
| Move Characteristic | `a07498ca-ad5b-474e-940d-263601a52153` | Read, Write, Notify | Carries a single-byte move value (`MOVE_*` encoding). The local side writes its move; the remote side receives it via notification. |

Protocol flow:

1. Both devices initially advertise the RPS service and scan for it.
2. When two devices discover each other, the lower-MAC device becomes Host (Peripheral), creates the service and characteristic, and starts advertising.
3. The higher-MAC device becomes Join (Central), scans, connects, discovers the service/characteristic, and enables notifications.
4. When the local player chooses a move in multiplayer mode, the firmware writes the move byte to the local characteristic value and, if acting as Peripheral, notifies the Central; if acting as Central, it writes the move byte directly to the Peripheral's characteristic.
5. When a notification arrives or a write response confirms the peer has sent a move, the peer move is stored and the round is evaluated once both moves are known.
6. In single-player mode, no BLE transmission occurs; the opponent move is generated locally after the local move is recorded.

### Persistence

- **No persistence in v0.1.0.** Round history and scores are stored only in RAM and reset to zero on every reboot.
- **Default storage:** none. No SD card, SPIFFS, or NVS usage in v0.1.0.
- If a future revision adds score persistence, it will use ESP32 NVS (no SD dependency).

---

## 5. API Function List

### Application Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `app_init` | `void app_init(void)` | `setup()` | Initialize serial, TFT, touch, BLE stack, LVGL, and UI. Logs `"CYD_RPS setup done"` on success. |
| `app_run` | `void app_run(void)` | `loop()` | Run LVGL timer handler, poll BLE events, dispatch state-machine events. Must not block. |
| `app_on_error` | `void app_on_error(const char* msg)` | Init or runtime failure | Transition to `SCR_Error` and log the error. |

### UI Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `ui_create` | `void ui_create(void)` | `app_init()` success | Create all screens and widgets and bind event handlers. |
| `ui_show_screen_start` | `void ui_show_screen_start(void)` | Boot complete, cancel, retry, or peer disconnect | Load `SCR_Start`. |
| `ui_show_screen_peer_search` | `void ui_show_screen_peer_search(void)` | Play button pressed | Load `SCR_PeerSearch` and reset progress/timeout labels. |
| `ui_show_screen_gameplay` | `void ui_show_screen_gameplay(void)` | Connected or new round or single-player fallback | Load `SCR_Gameplay`, enable move buttons, clear status. |
| `ui_show_screen_result` | `void ui_show_screen_result(move_t local, move_t peer, outcome_t outcome)` | Round evaluated | Load `SCR_Result` with move and outcome text. |
| `ui_show_screen_error` | `void ui_show_screen_error(const char* msg)` | Error event | Load `SCR_Error` with message. |
| `ui_set_status` | `void ui_set_status(const char* text)` | Connection/move events | Update the current screen's status label. |
| `ui_set_search_progress` | `void ui_set_search_progress(uint8_t percent)` | Discovery timer | Set `barSearch` value (0–100). |
| `ui_set_move_buttons_enabled` | `void ui_set_move_buttons_enabled(bool enabled)` | Move chosen / new round | Enable or disable `btnRock`, `btnPaper`, `btnScissors`. |

### BLE Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `ble_init` | `void ble_init(void)` | `app_init()` | Initialize the ESP32 BLE stack (NimBLE or Bluedroid). |
| `ble_start_discovery` | `void ble_start_discovery(void)` | UA-02 | Start peer discovery and role negotiation: advertise the RPS service and scan for peers; start the 10 s fallback timer. |
| `ble_stop_discovery` | `void ble_stop_discovery(void)` | UA-03 | Stop advertising/scanning and disconnect if connected. |
| `ble_disconnect` | `void ble_disconnect(void)` | Retry / peer disconnect | Drop the current BLE connection. |
| `ble_on_peer_found` | `void ble_on_peer_found(const uint8_t peer_addr[6])` | BLE scan/advertisement event | Callback invoked when a peer RPS device is discovered. |
| `ble_resolve_role` | `void ble_resolve_role(const uint8_t local_addr[6], const uint8_t peer_addr[6])` | `ble_on_peer_found()` | Compare addresses; set `role_t` to `ROLE_HOST` (lower MAC, Peripheral) or `ROLE_JOIN` (higher MAC, Central) and begin the appropriate connection procedure. |
| `ble_on_connected` | `void ble_on_connected(void)` | BLE connection event | Callback invoked when the BLE link is up and the GATT service is ready. |
| `ble_on_discovery_timeout` | `void ble_on_discovery_timeout(void)` | 10 s timer | Callback invoked when no peer is found; triggers single-player fallback. |
| `ble_send_move` | `bool ble_send_move(uint8_t move)` | UA-07/08/09 (multiplayer) | Transmit the local move byte to the peer. Returns true on success. |
| `ble_on_move_received` | `void ble_on_move_received(uint8_t move)` | BLE notification/write | Callback invoked when the peer's move arrives. |
| `ble_on_disconnected` | `void ble_on_disconnected(void)` | BLE disconnect event | Callback invoked when the peer disconnects. |
| `ble_is_connected` | `bool ble_is_connected(void)` | Any state query | Returns `ble_connected`. |

### Game Logic Layer

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `game_set_mode` | `void game_set_mode(game_mode_t mode)` | UA-05 / UA-06 | Set the current opponent source mode. |
| `game_set_local_move` | `void game_set_local_move(move_t move)` | UA-07/08/09 | Store the local move and check whether the round can be evaluated. |
| `game_set_peer_move` | `void game_set_peer_move(uint8_t move)` | UA-10/11 | Store and validate the peer move. |
| `game_set_opponent_random` | `void game_set_opponent_random(void)` | UA-07/08/09 single-player branch | Generate a uniform random opponent move (`MOVE_ROCK`, `MOVE_PAPER`, or `MOVE_SCISSORS`) and store it as the peer move. |
| `game_evaluate_round` | `outcome_t game_evaluate_round(void)` | Both moves known | Apply standard RPS rules and return `OUTCOME_WIN`, `OUTCOME_LOSE`, or `OUTCOME_DRAW`. |
| `game_reset_round` | `void game_reset_round(void)` | UA-05 / UA-12 | Clear local and peer moves for the next round. |
| `game_reset_all` | `void game_reset_all(void)` | UA-13 | Reset mode, role, connection, and game state to post-boot values. |
| `move_to_string` | `const char* move_to_string(move_t move)` | UI/serial logging | Map `MOVE_ROCK` → `"Rock"`, `MOVE_PAPER` → `"Paper"`, `MOVE_SCISSORS` → `"Scissors"`. |
| `outcome_to_string` | `const char* outcome_to_string(outcome_t outcome)` | UI/serial logging | Map outcome enum to `"WIN"`, `"LOSE"`, `"DRAW"`. |

### Event Handlers (LVGL callbacks)

| Function | Signature | Trigger | Description |
|----------|-----------|---------|-------------|
| `on_play_button_clicked` | `static void on_play_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnPlay` | Post `EV_PLAY` event. |
| `on_cancel_button_clicked` | `static void on_cancel_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnCancel` | Post `EV_CANCEL` event. |
| `on_rock_button_clicked` | `static void on_rock_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnRock` | Post `EV_MOVE_ROCK` event. |
| `on_paper_button_clicked` | `static void on_paper_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnPaper` | Post `EV_MOVE_PAPER` event. |
| `on_scissors_button_clicked` | `static void on_scissors_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnScissors` | Post `EV_MOVE_SCISSORS` event. |
| `on_play_again_button_clicked` | `static void on_play_again_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnPlayAgain` | Post `EV_PLAY_AGAIN` event. |
| `on_retry_button_clicked` | `static void on_retry_button_clicked(lv_event_t* e)` | `LV_EVENT_CLICKED` on `btnRetry` | Post `EV_RETRY` event. |

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
| BLE library | ESP32 built-in BLE (Arduino-ESP32) or NimBLE-Arduino | Use NimBLE-Arduino (`h2zero/NimBLE-Arduino@^1.4.0`) for smaller flash footprint; fallback to Bluedroid if needed. |
| Build tool | PlatformIO `pio run` | Default environment `esp32-2432s028r_cyd2usb_wokwi` produces `.pio/build/esp32-2432s028r_cyd2usb_wokwi/firmware.bin`; physical hardware uses `esp32-2432s028r_cyd2usb`. |
| Static analysis | `pio check` | Must report no critical/high defects before release. |
| Simulator | Wokwi CLI | `wokwi-cli` with `wokwi/diagram.json` and `wokwi/scenario.yaml`. Touch-driven tests are classified as `manual`/`hardware` per LL-032. |
| Serial monitor | 115200 baud | Standard CYD2USB rate. |

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

CYD_RPS v0.1.0 uses only onboard peripherals (display, touch, BLE radio). No external pins are required. The free pins remain available for future expansion:

- GPIO 22
- GPIO 27
- GPIO 35

### Wokwi Delta

- Wokwi uses the `wokwi-ili9341` part as a generic 240 × 320 framebuffer in place of the physical ST7789 panel.
- The `wokwi-xpt2046` part is documented in the hardware profile but is not supported by Wokwi CLI; touch-driven tests must be classified as `manual`/`hardware` per LL-032.
- Color byte order and gamma may differ from the physical board; visual correctness is validated on hardware.
- BLE pairing is not exercised in Wokwi CLI smoke tests; BLE functionality is validated on physical hardware or via host-level unit tests of the game logic.
- Single-player fallback can be exercised in Wokwi by injecting a discovery-timeout event or by asserting that the firmware enters gameplay after the timeout period.

### Power / Connection Notes

- Each CYD2USB board is powered via its USB-C connector.
- No wired connection between two boards is required; communication uses the ESP32 BLE radio.
- Keep the two boards within BLE range (typically ≤ 10 m indoors) during multiplayer play.
