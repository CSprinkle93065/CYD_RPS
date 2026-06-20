#pragma once

// BLE service for CYD_RPS.
//
// Implements:
//   - NimBLE stack initialization (call once from app_init).
//   - Concurrent advertise + scan peer discovery.
//   - Automatic role negotiation: lower public MAC becomes HOST (Peripheral),
//     higher MAC becomes JOIN (Central).
//   - Move send / receive over a single custom GATT characteristic.
//   - Non-blocking update() for discovery timeout and link monitoring.
//
// Radio startup (scan + advertise) runs on a dedicated FreeRTOS task so that
// NimBLE's call tree never shares the Arduino main loop / LVGL stack. The
// state machine signals the task to begin discovery; all events are still posted
// through the thread-safe sm_post_event() path.

#include <cstdint>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "game_logic.h"

namespace app {

// Custom GATT UUIDs from docs/definition.md.
extern const char* const RPS_SERVICE_UUID_STR;
extern const char* const RPS_MOVE_CHAR_UUID_STR;

class BleService {
public:
    BleService() = default;

    // Peer discovery timeout from docs/definition.md (milliseconds).
    static constexpr uint32_t DISCOVERY_TIMEOUT_MS = 10000;

    // Minimum free heap (bytes) required before building advertisement data.
    // NimBLEAdvertising::addServiceUUID() abort()s on a failed realloc; this
    // guard lets us fail gracefully and post EV_ERROR instead.
    static constexpr uint32_t MIN_HEAP_FOR_BLE_ADV = 8192;

    // Dedicated radio task stack size (words). Large enough to absorb the
    // NimBLE GAP/controller call depth without touching the main-task stack.
    static constexpr uint32_t RADIO_TASK_STACK_WORDS = 8192;

    // Number of JOIN->HOST connection attempts before giving up. v0.1.5 posted
    // EV_ERROR on the first connect() failure; v0.1.6 retries so the JOIN can
    // wait for the HOST to finish role resolution. See
    // docs/BugReport_CYD_RPS_v0.1.5.md §6.2 and §8.2.
    static constexpr int kConnectRetries = 4;

    // NimBLEClient::connect() default timeout is 30 s. When the controller
    // reports a connect failure with status=13 (BLE_HS_ETIMEOUT, "Operation
    // timed out."), waiting 30 s per retry makes the bounded retry window
    // unusable. Reducing the timeout to 5 s lets the JOIN retry quickly and
    // keeps total negotiation time under ~30 s including the discovery windows
    // below. See docs/BugReport_CYD_RPS_v0.1.6.md §8.6 and
    // docs/BugReport_CYD_RPS_v0.1.7.md §11.2.
    static constexpr uint8_t kConnectTimeoutSeconds = 5;

    // Discovery window the JOIN keeps advertising after role resolution before
    // stopping advertising and initiating the first connection attempt. Gives
    // the HOST multiple scan windows to discover the JOIN and resolve its own
    // role. See docs/BugReport_CYD_RPS_v0.1.7.md §9.1.
    static constexpr uint32_t kJoinDiscoveryWindowMs = 2000;

    // Inter-retry advertising window: on each failed JOIN->HOST connect attempt
    // (except the last), the JOIN restarts advertising for this duration before
    // the next connect() so the HOST gets another chance to discover it and stop
    // scanning. See docs/BugReport_CYD_RPS_v0.1.7.md §9.2.
    static constexpr uint32_t kJoinConnectInterRetryWindowMs = 2000;

    // Advertising interval during peer search / role negotiation (NimBLE units,
    // 1 unit = 0.625 ms). A shorter interval makes it more likely the peer's
    // scan window hits our advertisement within the bounded discovery window.
    // 32 units = 20 ms, 48 units = 30 ms; both are BLE-compliant.
    // See docs/BugReport_CYD_RPS_v0.1.7.md §9.3.
    static constexpr uint16_t kPeerSearchAdvMinInterval = 32;
    static constexpr uint16_t kPeerSearchAdvMaxInterval = 48;

    // Normal (relaxed) advertising interval used after role negotiation, e.g.
    // by the HOST once it has stopped scanning. 0x800 units = 1.28 s, matching
    // NimBLE's effective default advertising interval when 0 is requested.
    static constexpr uint16_t kNormalAdvMinInterval = 0x800;
    static constexpr uint16_t kNormalAdvMaxInterval = 0x800;

    // Test/development company ID placed at the start of the BLE advertisement
    // manufacturer-specific data. The full payload is company ID + 6-byte
    // public MAC. See docs/BugReport_CYD_RPS_v0.1.5.md §6.3 and §8.3.
    static constexpr uint16_t kManufacturerCompanyId = 0xFFFF;

    // Initialize NimBLE. Must be called from app_init before any other call.
    bool init();

    // Create the dedicated radio task. Call once after init(). Stage 13 wiring
    // may call this from main.cpp if preferred.
    void start_radio_task();

    // True once the radio task has been created and is ready for commands.
    bool radio_task_ready() const { return radio_task_handle_ != nullptr; }

    // Signal the radio task to begin peer discovery. On physical hardware this
    // avoids running NimBLE on the main task. Falls back to the direct path if
    // the radio task is not running (e.g. Wokwi simulation).
    void signal_discovery_start();

    // Signal the radio task to resolve the BLE role (HOST/JOIN) after a peer
    // has been discovered. Runs the address comparison and radio reconfiguration
    // on the dedicated task stack. Falls back to the direct path if the task is
    // not running.
    void signal_become_host();

    // Signal the radio task to connect to the resolved host peer. Runs the full
    // NimBLE client connection, GATT discovery, and subscription on the dedicated
    // task stack. Falls back to the direct path if the task is not running.
    void signal_connect_to_host();

    // Start peer discovery directly. Used by the radio task and as a Wokwi
    // fallback when no dedicated task is running.
    //
    // If NimBLE has not been initialized, the behavior is environment-specific:
    //   - WOKWI_SIMULATION: force the single-player fallback by making the
    //     discovery timeout expire on the next update() tick.
    //   - physical hardware: log an error and post EV_ERROR so the PUML error
    //     transition out of PeerSearch is exercised.
    void start_discovery();

    // Stop advertising/scanning and disconnect if connected. When the radio
    // task is running this posts a command to it; otherwise it stops directly.
    void stop_discovery();
    void disconnect();

    // Callback invoked from the NimBLE advertised-device callback.
    // Stores the peer address, address type, and (when available) the peer's
    // public MAC extracted from the manufacturer data. It ignores self-
    // discovered advertisements and posts EV_PEER_FOUND to move the state
    // machine into RoleNegotiating. The scanner callback supplies the
    // advertised address type and the public MAC parsed from the payload.
    void on_peer_found(const uint8_t peer_addr[6], uint8_t addr_type,
                       const uint8_t peer_public_addr[6]);

    // Resolve the role for the previously stored peer address and configure the
    // radio for HOST or JOIN. Posts EV_ROLE_RESOLVED on success. Called from
    // the AppStateMachine::on_peer_found() action so role resolution stays out
    // of the BLE interrupt/callback context.
    void resolve_role();

    // For the JOIN role: initiate the connection to the stored host address,
    // discover the remote move characteristic, subscribe, and post EV_CONNECTED
    // once the link is fully usable. Called from start_scanning_join().
    void connect_to_host();

    // Callbacks invoked from NimBLE server/client callbacks.
    void on_connected();
    void on_disconnected();
    void on_move_received(Move move);

    // Transmit the local move to the peer.
    bool send_move(Move move);

    // Non-blocking periodic processing: discovery timeout only. Radio startup
    // and deferred advertising now run on the dedicated task.
    void update(uint32_t now_ms);

    // Seconds remaining in the current discovery cycle, rounded up.
    // Returns 0xFF when no discovery cycle is active.
    uint8_t discovery_remaining_seconds() const;

    bool is_connected() const { return connected_; }
    bool is_discovering() const { return discovering_; }
    Move peer_move() const { return pending_peer_move_; }
    Role role() const { return role_; }

private:
    enum class RadioCommand : uint8_t {
        START_DISCOVERY,
        STOP_DISCOVERY,
        BECOME_HOST,      // Resolve HOST/JOIN role and configure radio accordingly.
        CONNECT_TO_HOST,  // Perform GATT connection/subscription as JOIN central.
    };

    void become_host();
    void become_join(const uint8_t peer_addr[6]);
    void post_peer_timeout_if_needed(uint32_t now_ms);
    void start_advertising_if_needed();

    bool start_scanning();
    bool start_advertising();
    void stop_scanning();
    void stop_advertising();

    // Internal direct variants (no command routing). Used by the radio task and
    // by code paths that already run on the correct context.
    void do_start_discovery();
    void do_stop_discovery();

    // Task body and entry point.
    static void radio_task_entry(void* param);
    void radio_task_loop();

    // Set when start_discovery() has started scanning but advertising has not
    // yet been started.  Cleared once advertising is started or when discovery
    // ends / role is resolved.
    bool advertising_pending_ = false;

    bool initialized_ = false;
    bool connected_ = false;
    bool discovering_ = false;
    Role role_ = Role::NONE;

    uint32_t discovery_start_ms_ = 0;
    bool peer_timeout_posted_ = false;

    // Peer move captured in a callback; copied into GameContext by the
    // state-machine action.
    Move pending_peer_move_ = Move::NONE;

    // Address of the first non-self peer discovered during the current
    // discovery cycle. Captured by on_peer_found() and consumed by
    // resolve_role() / connect_to_host().
    uint8_t peer_addr_[6] = {0};
    // BLE address type of the stored peer address (e.g. BLE_ADDR_PUBLIC or
    // BLE_ADDR_RANDOM). Captured from the advertisement because reconstructing
    // the NimBLEAddress with the wrong type causes connection status=13. See
    // docs/BugReport_CYD_RPS_v0.1.4.md §6.1 and §8.2.
    uint8_t peer_addr_type_ = 0;  // BLE_ADDR_PUBLIC by default
    bool peer_addr_valid_ = false;

    // Public MAC of the peer, extracted from the advertisement manufacturer-
    // specific data. This is the stable identifier used for symmetric role
    // resolution, while the random advertisement address (peer_addr_) is used
    // for the actual NimBLE connection. See docs/BugReport_CYD_RPS_v0.1.5.md
    // §6.3 and §8.3.
    uint8_t peer_public_addr_[6] = {0};
    bool peer_public_addr_valid_ = false;

    // NimBLE object handles (opaque to consumers).
    void* server_ = nullptr;
    void* service_ = nullptr;
    void* move_char_ = nullptr;
    void* advertising_ = nullptr;
    void* scan_ = nullptr;
    void* client_ = nullptr;
    void* remote_char_ = nullptr;

    // Dedicated radio task state.
    QueueHandle_t radio_cmd_queue_ = nullptr;
    TaskHandle_t radio_task_handle_ = nullptr;
};

// Global singleton accessor.
BleService& ble_service();

} // namespace app
