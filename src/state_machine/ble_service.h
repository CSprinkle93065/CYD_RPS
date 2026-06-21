#pragma once

// CYD_RPS v0.2.0 BLE service.
//
// Implements:
//   - NimBLE stack initialization (call once from app_init).
//   - Presence beacon on the Start screen (company ID 0xFF, 10-byte payload:
//     4-char device ID + 6-byte public MAC).
//   - User-selected Host mode: stop beacon/scan, set public address, create the
//     RPS GATT service/characteristic, advertise RPS service on public MAC,
//     and enforce a 15 s peer-join timeout.
//   - Auto-Join mode: detect a Host advertisement, stop beacon/scan, and
//     connect directly to the Host public MAC (BLE_ADDR_PUBLIC) with a 50 ms
//     guard after stopping advertising.
//   - Move send/receive over a single custom GATT characteristic.
//   - Dedicated FreeRTOS radio task: all NimBLE operations run from a command
//     queue.
//
// All NimBLE radio work runs on the dedicated task stack so that NimBLE's call
// tree never shares the Arduino main loop / LVGL stack. BLE callbacks and the
// state machine still post events through the thread-safe sm_post_event() path.

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

    // Host wait timeout (milliseconds).
    static constexpr uint32_t HOST_WAIT_TIMEOUT_MS = 15000;

    // Join connect timeout per attempt (seconds).
    static constexpr uint8_t JOIN_CONNECT_TIMEOUT_SEC = 5;

    // Guard delays from v0.2.0 BLE requirements.
    static constexpr uint32_t STOP_ADV_TO_CONNECT_GUARD_MS = 50;
    static constexpr uint32_t HOST_STOP_SCAN_TO_ADV_GUARD_MS = 20;

    // Advertising intervals (NimBLE units, 1 unit = 0.625 ms).
    static constexpr uint16_t PRESENCE_ADV_MIN_INTERVAL = 320;  // 200 ms
    static constexpr uint16_t PRESENCE_ADV_MAX_INTERVAL = 320;  // 200 ms
    static constexpr uint16_t HOST_ADV_MIN_INTERVAL = 160;      // 100 ms
    static constexpr uint16_t HOST_ADV_MAX_INTERVAL = 160;      // 100 ms

    // Minimum free heap before building advertisement data.
    static constexpr uint32_t MIN_HEAP_FOR_BLE_ADV = 8192;

    // Dedicated radio task stack size (words).
    static constexpr uint32_t RADIO_TASK_STACK_WORDS = 8192;

    // Company identifier used in the presence beacon manufacturer data.
    static constexpr uint16_t PRESENCE_COMPANY_ID = 0x00FF;

    // Initialize NimBLE. Must be called from main.cpp/app_init before any other
    // call. Under WOKWI_SIMULATION this returns true without touching NimBLE.
    bool init();

    // Create the dedicated radio task. Call once after init().
    void start_radio_task();

    // True once the radio task has been created and is ready for commands.
    bool radio_task_ready() const { return radio_task_handle_ != nullptr; }

    // -----------------------------------------------------------------------
    // Direct (non-task) API. Safe to call from any context; when the radio task
    // is running the signal_* variants below are preferred.
    // -----------------------------------------------------------------------
    void start_presence_beacon();
    void stop_presence_beacon();
    void stop_host_advertising();
    void restart_host_advertising();
    void become_host();
    void connect_to_host(const uint8_t host_public_mac[6]);

    // -----------------------------------------------------------------------
    // Radio-task command queue API.
    // -----------------------------------------------------------------------
    void signal_start_presence_beacon();
    void signal_stop_presence_beacon();
    void signal_stop_host_advertising();
    void signal_restart_host_advertising();
    void signal_become_host();
    void signal_connect_to_host(const uint8_t host_public_mac[6]);

    // Called when a Host presence beacon is discovered while on Start.
    void on_host_found(const uint8_t host_public_mac[6]);

    // Callbacks invoked from NimBLE server/client callbacks.
    void on_connected();
    void on_disconnected();
    void on_move_received(Move move);

    // Disconnect if connected; stop advertising/scan.
    void disconnect();

    // Transmit the local move to the peer.
    bool send_move(Move move);

    // Non-blocking periodic processing (host timeout).
    void update(uint32_t now_ms);

    // Host-wait timeout helpers used by AppStateMachine::update().
    uint8_t host_wait_percent_remaining() const;
    bool host_wait_timed_out() const;
    void mark_host_wait_timeout_posted();

    // Device ID helpers (4-hex-digit string derived from public MAC).
    void get_device_id(char* out, size_t len) const;
    void get_peer_device_id(char* out, size_t len) const;

    // State accessors.
    bool is_initialized() const { return initialized_; }
    bool is_connected() const { return connected_; }
    Move peer_move() const { return pending_peer_move_; }
    Role role() const { return role_; }

private:
    enum class RadioCommandType : uint8_t {
        START_PRESENCE,
        STOP_PRESENCE,
        STOP_HOST,
        RESTART_HOST,
        BECOME_HOST,
        CONNECT_TO_HOST,
    };

    struct RadioCommand {
        RadioCommandType type;
        uint8_t mac[6];
    };

    // Helpers used by direct and radio-task paths.
    void do_start_presence_beacon();
    void do_stop_presence_beacon();
    void do_stop_host_advertising();
    void do_restart_host_advertising();
    void do_become_host();
    void do_connect_to_host(const uint8_t host_public_mac[6]);

    void start_host_advertising();
    void start_presence_advertising();
    void stop_scanning();
    void stop_advertising();

    bool start_scanning();
    bool start_advertising_common(uint16_t min_interval, uint16_t max_interval,
                                  bool include_presence_manu_data);

    void post_host_timeout_if_needed(uint32_t now_ms);

    // Task body and entry point.
    static void radio_task_entry(void* param);
    void radio_task_loop();

    bool initialized_ = false;
    bool connected_ = false;
    bool presence_active_ = false;
    bool host_active_ = false;
    Role role_ = Role::NONE;

    // Host wait timer.
    uint32_t host_wait_start_ms_ = 0;
    bool host_wait_timeout_posted_ = false;

    // Peer move captured in a callback; copied into GameContext by the
    // state-machine action.
    Move pending_peer_move_ = Move::NONE;

    // Addresses.
    uint8_t public_mac_[6] = {0};
    uint8_t peer_public_mac_[6] = {0};
    bool peer_public_mac_valid_ = false;

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
