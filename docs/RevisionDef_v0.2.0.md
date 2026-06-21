# CYD_RPS v0.2.0 Revision Definition

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Status:** Draft — decisions captured, ready for implementation planning  
**Date:** 2026-06-18  

---

## 1. Purpose

This document defines the CYD_RPS v0.2.0 revision. It replaces the v0.1.x automatic MAC-address role negotiation with an explicit, user-selected **Host** model, refreshes the UI to a white-on-black theme, adds a session scoreboard, and fixes the two-player connection path that produced repeated BLE `status=13` failures in v0.1.9.

---

## 2. Decision Log

| ID | Topic | Decision | Rationale |
|----|-------|----------|-----------|
| D01 | Connection model | Use a **user-selected host** model. The first user who taps **"Host Game"** makes that device the **Host** (BLE Peripheral / advertiser). The peer device becomes the **Join** (BLE Central / scanner) and connects automatically. | Removes the public-MAC tie-breaker and the advertising-restart race that caused `status=13`. Gives users explicit control. |
| D02 | Discovery radio | Use **BLE presence beacons** for device discovery and the existing **BLE GATT connection** for the game move exchange. | Reuses the NimBLE stack already in the project; discovery becomes visible and user-driven. |
| D03 | Start screen controls | Show two buttons: **"Host Game"** and **"Solo"**. The peer device auto-detects a Host and joins without a separate button press. | Simplifies the second-device UX to "wait for the Host to start." |
| D04 | Host timeout / fallback | If no peer joins a Host session before the timeout, show a dialog with **"Retry"** and **"Solo"**. Retry continues hosting; Solo enters single-player mode. | Avoids the hidden auto-fallback and gives the user a clear choice. |
| D05 | Visual theme | Full UI redesign with a **white-on-black** color scheme. No more white text on colored button backgrounds. | Addresses the readability complaint and gives v0.2.0 a distinct look. |
| D06 | Scoreboard | Show a **session scoreboard** (wins / losses / draws) across rounds. Scores are RAM-only and reset on reboot or firmware reset. | Adds motivation for repeat rounds without adding persistence complexity. |
| D07 | Global home/reset button | Place a small **home icon** button in the **upper-right corner** of every screen. Tapping it performs a firmware reset (`ESP.restart()`). | Makes it easy to recover from any state without power cycling. |
| D08 | BLE address handling | The **Host must advertise using its public MAC address**. The **Join must connect using that public MAC address**, not a reconstructed address from `getNative()`. | Eliminates the byte-order mismatch and address-rotation problems identified in `BugReport_CYD_RPS_v0.1.9.md`. |

---

## 3. Application Overview

CYD_RPS v0.2.0 is still a two-player Rock-Paper-Scissors game for CYD2USB v3 boards, but the way two boards find each other changes:

- On boot, each board initializes BLE and begins sending a short **presence beacon** that contains a human-readable device ID.
- The start screen offers **Host Game** and **Solo**.
- Tapping **Host Game** makes this board the Host. It stops sending presence beacons, starts advertising the RPS game service, and waits for a Joiner.
- A peer board that is still on its start screen scans for Host advertisements. When it sees one, it becomes the Join, stops its own beacon, and connects to the Host.
- Tapping **Solo** skips all peer discovery and starts a single-player round against the RNG opponent.
- If a Host waits too long without a Joiner, a dialog offers **Retry** (keep hosting) or **Solo** (play alone).
- After each round, a scoreboard shows cumulative wins/losses/draws for the current session. A **home icon** in the top-right corner restarts the device from any screen.

### Scope

- Two-device BLE multiplayer with explicit Host selection.
- Single-player mode against a uniform RNG opponent.
- Session scoreboard (RAM-only).
- No persistent storage.
- No audio, no LED feedback, minimal animation (screen transitions, progress bar, spinner optional).

---

## 4. BLE Topology

### Presence Beacon

- Both devices advertise a manufacturer-specific data payload or a secondary service UUID containing:
  - A short device ID (e.g., the last two bytes of the public MAC rendered as four hex digits, such as `8D90`).
  - The full public MAC address of the device, so a Joiner can connect directly to a stable, known address.
- The beacon advertisement interval should be fast enough for quick discovery (e.g., 100–300 ms) but low enough to avoid congestion.
- The beacon is sent only while the device is on the start screen and has not yet resolved a role.

### Host Mode

- Triggered by tapping **Host Game**.
- The device:
  1. Stops the presence beacon.
  2. Stops scanning.
  3. Sets its own address type to public (`BLE_OWN_ADDR_PUBLIC`) so it advertises and accepts connections on its public MAC.
  4. Creates the RPS GATT service and Move characteristic if not already created.
  5. Starts advertising the RPS service UUID on the public MAC.
  6. Starts a connection timeout timer.

### Join Mode

- Triggered automatically when the start-screen scan detects a Host advertisement.
- The device:
  1. Stops its own presence beacon.
  2. Stops scanning.
  3. Extracts the Host's public MAC from the presence beacon (or from the advertisement address if it is already public).
  4. Constructs the connect address directly from the public MAC string or uses the `NimBLEAddress` object returned by the scan without calling `getNative()`.
  5. Connects to the Host public MAC.
  6. Discovers the RPS service and Move characteristic.
  7. Enables notifications.

### Simultaneous Host Conflict Resolution

If both users tap **Host Game** at nearly the same time:

1. Each device continues scanning while advertising as Host.
2. If a device sees a peer-Host advertisement, it immediately cancels its own Host attempt and becomes Join.
3. As a safety net, if both devices end up advertising as Host, the device with the **lower BLE MAC address** remains Host and the other becomes Join.

### Connection Timeout

- Host timeout default: **15 seconds** (configurable constant).
- On timeout, show a modal dialog:
  - Title: `"No peer joined"`
  - Buttons: **Retry** (restart Host advertising and timer), **Solo** (enter single-player mode).
- Tapping **Cancel** on the hosting screen returns to the start screen.

---

## 5. UI/Display Layout

### Screen Properties

- **Resolution:** 240 × 320 portrait (USB connector at the bottom).
- **Background:** black (`#000000`).
- **Primary text:** white (`#FFFFFF`).
- **Accent:** a single accent color (e.g., cyan `#00BCD4` or green `#4CAF50`) used sparingly for active states and highlights.
- **Font:** Montserrat family; large sizes for readability.

### Global Home Button

Every screen includes a small home icon button in the upper-right corner:

| Control | Type | Position | Behavior |
|---------|------|----------|----------|
| `btnHome` | `lv_btn` | Top-right, x ≈ 200 px, y ≈ 10 px, 30 × 30 px | Tapping calls `ESP.restart()` |
| `lblHomeIcon` | `lv_label` inside button | Centered | Unicode home icon `🏠` or `⌂` |

### SCR_Start — Start Screen

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 40 px | `"CYD RPS"` |
| `lblDeviceId` | `lv_label` | Below title, y ≈ 80 px | `"Device: 8D90"` (last 4 hex digits of MAC) |
| `lblStatus` | `lv_label` | y ≈ 120 px | `"Searching for host..."` if a Host is nearby; otherwise `"Ready"` |
| `btnHostGame` | `lv_btn` | Center, y ≈ 170 px, 180 × 60 px | `"Host Game"` — become Host |
| `btnSolo` | `lv_btn` | Below Host, y ≈ 245 px, 180 × 50 px | `"Solo"` — single-player mode |

When a Join device detects a Host beacon, the status label changes to `"Host found — joining..."` and the buttons are disabled until the connection resolves or fails.

### SCR_HostWait — Hosting / Waiting for Peer (new screen)

Shown after tapping **Host Game**.

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 40 px | `"Hosting Game"` |
| `lblDeviceId` | `lv_label` | y ≈ 80 px | `"Device: 8D90"` |
| `lblStatus` | `lv_label` | y ≈ 120 px | `"Waiting for peer to join..."` |
| `barProgress` | `lv_bar` | y ≈ 160 px, 180 × 20 px | Timeout progress (100% → 0%) |
| `btnCancel` | `lv_btn` | Bottom-center, y ≈ 240 px, 120 × 50 px | `"Cancel"` — return to start screen |

On timeout, a modal dialog appears over this screen with **Retry** and **Solo**.

### SCR_Gameplay — Move Selection

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 30 px | `"Choose!"` |
| `lblScore` | `lv_label` | Top-left, y ≈ 30 px | `"W:0 L:0 D:0"` |
| `btnHome` | home button | Top-right | Reset firmware |
| `lblStatus` | `lv_label` | y ≈ 70 px | `"Waiting for your move"` |
| `btnRock` | `lv_btn` | y ≈ 110 px, 180 × 50 px | `"Rock"` |
| `btnPaper` | `lv_btn` | y ≈ 170 px, 180 × 50 px | `"Paper"` |
| `btnScissors` | `lv_btn` | y ≈ 230 px, 180 × 50 px | `"Scissors"` |

Move buttons use a subtle white border on black; the active/pressed state uses the accent color.

### SCR_Result — Round Result

| Control | Type | Position / Size | Text / Behavior |
|---------|------|-----------------|-----------------|
| `lblTitle` | `lv_label` | Top-center, y ≈ 30 px | `"Result"` |
| `lblScore` | `lv_label` | Top-left, y ≈ 30 px | `"W:1 L:0 D:0"` |
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

---

## 6. User Actions

| ID | Action | Input | System Response |
|----|--------|-------|-----------------|
| UA-01 | Power on / boot | Reset | Initialize hardware, BLE, LVGL, UI; start presence beacon and scan; show SCR_Start. |
| UA-02 | Host a game | Tap `btnHostGame` | Stop beacon, start RPS service advertisement, show SCR_HostWait, start timeout. |
| UA-03 | Play solo | Tap `btnSolo` | Stop BLE scan/advertise, set single-player mode, show SCR_Gameplay. |
| UA-04 | Peer auto-joins | Host advertisement detected while on SCR_Start | Stop beacon/scan, connect to Host, show SCR_Gameplay on success. |
| UA-05 | Cancel hosting | Tap `btnCancel` on SCR_HostWait | Stop advertising, restart presence beacon/scan, return to SCR_Start. |
| UA-06 | Host timeout | Timer expires without peer | Show Retry/Solo dialog. |
| UA-07 | Retry hosting | Tap **Retry** in timeout dialog | Restart Host advertising and timeout. |
| UA-08 | Timeout → solo | Tap **Solo** in timeout dialog | Enter single-player mode, show SCR_Gameplay. |
| UA-09 | Choose Rock / Paper / Scissors | Tap move button | Record local move, send/receive over BLE or generate opponent move, disable move buttons. |
| UA-10 | Play again | Tap `btnPlayAgain` | Reset round, re-enable move buttons, show SCR_Gameplay. |
| UA-11 | Firmware reset | Tap home icon on any screen | Call `ESP.restart()`. |

---

## 7. Data Model Updates

### New/Updated Runtime Data

| Name | Type | Description |
|------|------|-------------|
| `device_id_str` | `char[5]` | Four-hex-digit local device ID derived from the public MAC (e.g., `"8D90"`). |
| `peer_device_id_str` | `char[5]` | Device ID of the connected peer (multiplayer only). |
| `session_score_t` | `struct { uint16_t wins; uint16_t losses; uint16_t draws; }` | Cumulative round results for the current power-on session. |
| `role_t` | `enum { ROLE_NONE, ROLE_HOST, ROLE_JOIN }` | Current BLE role. `ROLE_NEGOTIATING` is removed because roles are now user-selected or auto-joined. |
| `game_mode_t` | `enum { MODE_SINGLE_PLAYER, MODE_MULTI_PLAYER }` | Unchanged from v0.1.x. |
| `move_t` / `outcome_t` | enums | Unchanged. |

### Score Update Rules

- After each round, increment the appropriate counter based on the local outcome:
  - `OUTCOME_WIN`  → `wins++`
  - `OUTCOME_LOSE` → `losses++`
  - `OUTCOME_DRAW` → `draws++`
- Scores reset to zero on boot or firmware reset.
- No persistence across reboots.

---

## 8. API Function Changes

### New UI Functions

| Function | Description |
|----------|-------------|
| `ui_show_screen_host_wait(void)` | Load the hosting/waiting screen. |
| `ui_show_host_timeout_dialog(void)` | Show the Retry/Solo dialog over SCR_HostWait. |
| `ui_hide_host_timeout_dialog(void)` | Dismiss the Retry/Solo dialog. |
| `ui_set_device_id(const char* id)` | Update the device ID label on SCR_Start and SCR_HostWait. |
| `ui_set_peer_device_id(const char* id)` | Show the peer device ID in status text (optional). |
| `ui_set_score(uint16_t wins, uint16_t losses, uint16_t draws)` | Update the score label on gameplay/result screens. |
| `ui_add_home_button(lv_obj_t* screen)` | Helper to add the home reset button to any screen. |

### Modified BLE Functions

| Function | Change |
|----------|--------|
| `ble_start_discovery` | Renamed or replaced by `ble_start_presence_beacon` + `ble_start_host_advertising`. |
| `ble_resolve_role` | Removed; role is now set by user action or auto-join detection. |
| `ble_on_peer_found` | Now used to detect a Host advertisement while on the start screen. |
| `ble_on_host_selected` | New: called when user taps Host Game. |
| `ble_on_join_detected` | New: called when a Host beacon is seen and this device should connect. |

---

## 9. Visual States Summary

- **Boot:** black screen, no UI visible.
- **Start:** title, local device ID, status, Host Game button, Solo button, home icon.
- **Hosting:** title, device ID, progress bar, Cancel button, home icon.
- **Hosting timeout dialog:** modal overlay with Retry and Solo buttons.
- **Joining:** start screen shows "Host found — joining..." briefly, then transitions to gameplay.
- **Gameplay:** score label, status, three move buttons, home icon.
- **Result:** score label, local/peer moves, outcome, Play Again button, home icon.
- **Error:** error title/message, Retry button, home icon.

---

## 10. Open Questions / Implementation Notes

1. **Host timeout duration:** Default is 15 s; may be tuned during hardware testing.
2. **Presence beacon format:** Decide whether to use a custom service UUID or manufacturer data. Must not conflict with the RPS game service UUID and must carry the Host public MAC.
3. **Public-address advertising:** Verify that `BLE_OWN_ADDR_PUBLIC` works on both physical CYD2USB v3 boards and in Wokwi simulation.
4. **Move button visual style:** White outline on black, accent fill on press. Icons (emoji) may be removed if they clash with the new theme.
5. **Score label visibility:** The scoreboard appears on gameplay and result screens; consider also showing it on the start screen for the previous session (not applicable because scores reset on boot).
6. **Home button safe reset:** Ensure `ESP.restart()` is called cleanly from the LVGL event context; defer to the main loop if needed.

---

## 11. Toolchain / Build

Unchanged from v0.1.x unless ESP-NOW is introduced later. Current plan remains:

- Arduino-ESP32 framework
- LVGL 8.4.0
- TFT_eSPI 2.5.43
- XPT2046_Touchscreen
- NimBLE-Arduino 1.4.x
- PlatformIO build environments for Wokwi and physical CYD2USB v3

---

## 12. References

- `projects/CYD_RPS/docs/BugReport_CYD_RPS_v0.1.9.md` — Root-cause analysis of the v0.1.9 `status=13` failure. The byte-order mismatch and address-rotation findings led directly to Decision D08.
- `projects/CYD_RPS/docs/definition.md` — Original v0.1.0 project definition (screens, data model, API baseline).

*End of Revision Definition v0.2.0*
