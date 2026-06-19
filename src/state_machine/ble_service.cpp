#include "ble_service.h"

#include <Arduino.h>

#include <NimBLEDevice.h>
#include <string>
#include <vector>

#include "app_state_machine.h"
#include "hal_config.h"

namespace app {

const char* const RPS_SERVICE_UUID_STR = "a07498ca-ad5b-474e-940d-263601a52152";
const char* const RPS_MOVE_CHAR_UUID_STR = "a07498ca-ad5b-474e-940d-263601a52153";



// Convenience casts from the opaque void* handles stored in BleService.
static inline NimBLEServer* server_ptr(void* p) { return reinterpret_cast<NimBLEServer*>(p); }
static inline NimBLEService* service_ptr(void* p) { return reinterpret_cast<NimBLEService*>(p); }
static inline NimBLECharacteristic* char_ptr(void* p) { return reinterpret_cast<NimBLECharacteristic*>(p); }
static inline NimBLEAdvertising* adv_ptr(void* p) { return reinterpret_cast<NimBLEAdvertising*>(p); }
static inline NimBLEScan* scan_ptr(void* p) { return reinterpret_cast<NimBLEScan*>(p); }
static inline NimBLEClient* client_ptr(void* p) { return reinterpret_cast<NimBLEClient*>(p); }
static inline NimBLERemoteCharacteristic* remote_char_ptr(void* p) { return reinterpret_cast<NimBLERemoteCharacteristic*>(p); }

// ---------------------------------------------------------------------------
// NimBLE callbacks (post events to the engine; no blocking work)
// ---------------------------------------------------------------------------

class RpsServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* /*pServer*/) override {
        ble_service().on_connected();
    }
    void onDisconnect(NimBLEServer* /*pServer*/) override {
        ble_service().on_disconnected();
    }
};

class RpsCharCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        NimBLEAttValue val = pCharacteristic->getValue();
        if (val.length() > 0) {
            ble_service().on_move_received(static_cast<Move>(val[0]));
        }
    }
};

class RpsAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        static const NimBLEUUID service_uuid(RPS_SERVICE_UUID_STR);
        if (advertisedDevice->isAdvertisingService(service_uuid)) {
            uint8_t addr[6];
            memcpy(addr, advertisedDevice->getAddress().getNative(), 6);
            // Capture the address type as well as the bytes. Reconstructing
            // the NimBLEAddress with BLE_ADDR_PUBLIC when the peer advertised a
            // random address causes connection status=13. See
            // docs/BugReport_CYD_RPS_v0.1.4.md §6.1 and §8.2.

            // The peer's public MAC is carried in the manufacturer-specific
            // data so both sides can resolve roles with a stable, symmetric
            // identifier instead of the random advertisement address. See
            // docs/BugReport_CYD_RPS_v0.1.5.md §6.3 and §8.3.
            uint8_t public_addr[6] = {0};
            bool public_addr_valid = false;
            const std::string manu = advertisedDevice->getManufacturerData();
            if (manu.length() >= 8 &&
                static_cast<uint8_t>(manu[0]) == (BleService::kManufacturerCompanyId & 0xFF) &&
                static_cast<uint8_t>(manu[1]) == ((BleService::kManufacturerCompanyId >> 8) & 0xFF)) {
                memcpy(public_addr, manu.data() + 2, 6);
                public_addr_valid = true;
            }

            ble_service().on_peer_found(addr, advertisedDevice->getAddress().getType(),
                                        public_addr_valid ? public_addr : nullptr);
        }
    }
};

class RpsClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* /*pClient*/) override {
        ble_service().on_connected();
    }
    void onDisconnect(NimBLEClient* /*pClient*/) override {
        ble_service().on_disconnected();
    }
};

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

static BleService s_ble_service;

BleService& ble_service() {
    return s_ble_service;
}

// ---------------------------------------------------------------------------
// Initialization / discovery
// ---------------------------------------------------------------------------

bool BleService::init() {
    if (initialized_) {
        return true;
    }

    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Create the GATT server and move characteristic up-front so both roles
    // can share the same attribute handle layout.
    server_ = NimBLEDevice::createServer();
    NimBLEServer* srv = server_ptr(server_);
    if (!srv) {
        return false;
    }
    srv->setCallbacks(new RpsServerCallbacks());

    service_ = srv->createService(RPS_SERVICE_UUID_STR);
    if (!service_ptr(service_)) {
        return false;
    }

    move_char_ = service_ptr(service_)->createCharacteristic(
        RPS_MOVE_CHAR_UUID_STR,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    if (!char_ptr(move_char_)) {
        return false;
    }
    char_ptr(move_char_)->setCallbacks(new RpsCharCallbacks());
    if (!service_ptr(service_)->start()) {
        return false;
    }

    // Start the GATT server now, while no GAP scan/advertise/connection
    // procedure is active. NimBLEAdvertising::start() lazily calls
    // NimBLEServer::start() if m_gattsStarted is false; if that lazy start
    // happens while discovery is already active, ble_gatts_start() returns
    // BLE_HS_EBUSY (rc=15) and the library abort()s. See
    // docs/BugReport_CYD_RPS_v0.1.2.md §6 and §8.1.
    server_ptr(server_)->start();

    initialized_ = true;
    return true;
}

void BleService::start_radio_task() {
    if (radio_task_handle_ != nullptr) {
        return;
    }
    if (radio_cmd_queue_ == nullptr) {
        radio_cmd_queue_ = xQueueCreate(4, sizeof(RadioCommand));
    }
    if (radio_cmd_queue_ == nullptr) {
        Serial.println("ERROR: BLE radio command queue creation failed");
        return;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        radio_task_entry,
        "ble_radio",
        RADIO_TASK_STACK_WORDS,
        this,
        1,
        &radio_task_handle_,
        0); // run on the application / non-LVGL core if possible
    if (rc != pdPASS) {
        Serial.println("ERROR: BLE radio task creation failed");
        radio_task_handle_ = nullptr;
    }
}

void BleService::signal_discovery_start() {
    if (!radio_task_ready()) {
        start_discovery();
        return;
    }
    RadioCommand cmd = RadioCommand::START_DISCOVERY;
    if (xQueueSend(radio_cmd_queue_, &cmd, 0) != pdPASS) {
        // Queue full (should never happen with a 4-slot queue). Fall back to
        // the direct path so the game does not silently stall.
        Serial.println("ERROR: BLE radio command queue full");
        start_discovery();
    }
}

void BleService::signal_become_host() {
    if (!radio_task_ready()) {
        resolve_role();
        return;
    }
    RadioCommand cmd = RadioCommand::BECOME_HOST;
    if (xQueueSend(radio_cmd_queue_, &cmd, 0) != pdPASS) {
        // Queue full - run directly on the main task as a safety fallback.
        Serial.println("ERROR: BLE radio command queue full");
        resolve_role();
    }
}

void BleService::signal_connect_to_host() {
    if (!radio_task_ready()) {
        connect_to_host();
        return;
    }
    RadioCommand cmd = RadioCommand::CONNECT_TO_HOST;
    if (xQueueSend(radio_cmd_queue_, &cmd, 0) != pdPASS) {
        // Queue full - run directly on the main task as a safety fallback.
        Serial.println("ERROR: BLE radio command queue full");
        connect_to_host();
    }
}

void BleService::start_discovery() {
    do_start_discovery();
}

void BleService::do_start_discovery() {
    role_ = Role::NONE;
    connected_ = false;
    discovering_ = true;
    peer_timeout_posted_ = false;
    pending_peer_move_ = Move::NONE;
    peer_addr_valid_ = false;
    peer_addr_type_ = 0;  // BLE_ADDR_PUBLIC default for each new discovery cycle
    peer_public_addr_valid_ = false;
    advertising_pending_ = false;
    memset(peer_addr_, 0, sizeof(peer_addr_));
    memset(peer_public_addr_, 0, sizeof(peer_public_addr_));

    Serial.printf("BLE: start_discovery heap=%u\n", ESP.getFreeHeap());

    if (!initialized_) {
#if defined(WOKWI_SIMULATION)
        // Wokwi does not emulate NimBLE, so the machine cannot discover a real
        // peer. Force the single-player fallback path by making the polled
        // timeout expire on the very next update() tick.
        Serial.println("CYD_RPS: Wokwi - BLE skipped, forcing single-player fallback");
        discovery_start_ms_ = millis() - BleService::DISCOVERY_TIMEOUT_MS;
#else
        // On physical hardware this is an unexpected failure: NimBLE should
        // have been initialized by main.cpp. Route through the PUML error
        // transition instead of silently stalling in PeerSearch.
        Serial.println("ERROR: BLE not initialized");
        discovering_ = false;
        sm_post_event(Event::EV_ERROR);
#endif
        return;
    }

    // Start scanning for peers advertising the RPS service.
    // Use the non-blocking overload (callback-based API) and let the state-
    // machine update loop drive the 10 s timeout via post_peer_timeout_if_needed().
    if (!start_scanning()) {
        Serial.println("ERROR: BLE scan start failed");
        do_stop_discovery();
        discovering_ = false;
        sm_post_event(Event::EV_ERROR);
        return;
    }

    // Defer advertising startup by one cycle. In the radio-task path the task
    // immediately calls start_advertising_if_needed() after this returns, so
    // the actual advertise start still happens on the dedicated task stack.
    advertising_pending_ = true;
    discovery_start_ms_ = millis();
}

void BleService::stop_discovery() {
    if (!radio_task_ready()) {
        do_stop_discovery();
        return;
    }
    RadioCommand cmd = RadioCommand::STOP_DISCOVERY;
    if (xQueueSend(radio_cmd_queue_, &cmd, 0) != pdPASS) {
        // Queue full - stop directly as a safety fallback.
        do_stop_discovery();
    }
}

void BleService::do_stop_discovery() {
    stop_scanning();
    stop_advertising();
    discovering_ = false;
    advertising_pending_ = false;
    peer_addr_valid_ = false;
    peer_addr_type_ = 0;  // BLE_ADDR_PUBLIC default
    peer_public_addr_valid_ = false;
    memset(peer_addr_, 0, sizeof(peer_addr_));
    memset(peer_public_addr_, 0, sizeof(peer_public_addr_));
}

void BleService::disconnect() {
    if (client_) {
        client_ptr(client_)->disconnect();
    }
    if (server_) {
        server_ptr(server_)->disconnect(0, 0);
    }
    connected_ = false;
}

// ---------------------------------------------------------------------------
// Peer discovery callback and role resolution
// ---------------------------------------------------------------------------

void BleService::on_peer_found(const uint8_t peer_addr[6], uint8_t addr_type,
                               const uint8_t peer_public_addr[6]) {
    if (!peer_addr) {
        return;
    }

    // Already resolved or captured a peer for this cycle.
    if (role_ != Role::NONE || peer_addr_valid_) {
        return;
    }

    // Self-discovery guard: ignore any advertisement whose stable identifier
    // matches the local address. Prefer the public MAC carried in the
    // manufacturer data; fall back to the random advertisement address if the
    // payload is not present. See docs/BugReport_CYD_RPS_v0.1.5.md §6.3.
    NimBLEAddress local_addr = NimBLEDevice::getAddress();
    const uint8_t* local = local_addr.getNative();
    if (peer_public_addr) {
        if (memcmp(local, peer_public_addr, 6) == 0) {
            return;
        }
        memcpy(peer_public_addr_, peer_public_addr, 6);
        peer_public_addr_valid_ = true;
    } else {
        peer_public_addr_valid_ = false;
        if (memcmp(local, peer_addr, 6) == 0) {
            return;
        }
    }

    memcpy(peer_addr_, peer_addr, 6);
    peer_addr_type_ = addr_type;
    peer_addr_valid_ = true;

    // Move the machine from PeerSearch to RoleNegotiating; actual role
    // resolution is performed by resolve_role() on the dedicated radio task.
    sm_post_event(Event::EV_PEER_FOUND);
}

void BleService::resolve_role() {
    if (!peer_addr_valid_ || role_ != Role::NONE) {
        return;
    }

    const NimBLEAddress local_addr = NimBLEDevice::getAddress();
    const uint8_t* local = local_addr.getNative();

    // Use the stable public MAC from the advertisement payload for symmetric
    // role resolution; fall back to the random advertisement address only if
    // the peer did not include manufacturer data. See
    // docs/BugReport_CYD_RPS_v0.1.5.md §6.3 and §8.3.
    const uint8_t* peer_id = peer_addr_;
    if (peer_public_addr_valid_) {
        peer_id = peer_public_addr_;
    }

    const int cmp = memcmp(local, peer_id, 6);
    if (cmp == 0) {
        // Should have been filtered by on_peer_found(), but ignore safely.
        return;
    }

    if (cmp < 0) {
        become_host();
    } else {
        become_join(peer_addr_);
    }
}

void BleService::become_host() {
    role_ = Role::HOST;
    advertising_pending_ = false;
    // Host is the Peripheral: stop scanning and make sure advertising is running.
    stop_scanning();
    // start_advertising() returns immediately without restarting if advertising
    // is already active. This prevents regenerating the random advertisement
    // address, which would invalidate the address the JOIN captured during
    // discovery. See docs/BugReport_CYD_RPS_v0.1.5.md §6.4 and §8.4.
    if (!start_advertising()) {
        Serial.println("ERROR: BLE host advertise start failed");
        do_stop_discovery();
        discovering_ = false;
        sm_post_event(Event::EV_ERROR);
        return;
    }
    sm_post_event(Event::EV_ROLE_RESOLVED);
}

void BleService::become_join(const uint8_t peer_addr[6]) {
    role_ = Role::JOIN;
    advertising_pending_ = false;
    // Join is the Central: stop scanning so it can drive the connection, but
    // KEEP ADVERTISING so the peer (which will become HOST) can still discover
    // this unit and resolve its own role. Stopping advertising here caused the
    // v0.1.5 asymmetric role-negotiation failure. See
    // docs/BugReport_CYD_RPS_v0.1.5.md §6.1 and §8.1.
    stop_scanning();
    // Do NOT call stop_advertising() here.
    memcpy(peer_addr_, peer_addr, 6);
    peer_addr_valid_ = true;
    sm_post_event(Event::EV_ROLE_RESOLVED);
}

void BleService::connect_to_host() {
    if (role_ != Role::JOIN || !peer_addr_valid_) {
        sm_post_event(Event::EV_ERROR);
        return;
    }

    client_ = NimBLEDevice::createClient();
    NimBLEClient* cli = client_ptr(client_);
    if (!cli) {
        sm_post_event(Event::EV_ERROR);
        return;
    }
    cli->setClientCallbacks(new RpsClientCallbacks());

    // The JOIN must stop advertising before acting as a central. The ESP32
    // NimBLE controller rejects the connect attempt with status=13
    // (BLE_ERR_CONN_REJ_RESOURCES) when the JOIN remains a peripheral
    // advertiser while initiating a connection. See
    // docs/BugReport_CYD_RPS_v0.1.6.md §8.5.
    stop_advertising();

    uint8_t addr_buf[6];
    memcpy(addr_buf, peer_addr_, 6);
    // Use the address type captured during discovery; reconstructing with
    // BLE_ADDR_PUBLIC causes status=13 when the peer advertised a random or
    // resolvable address. See docs/BugReport_CYD_RPS_v0.1.4.md §6.2 and §8.3.
    NimBLEAddress addr(addr_buf, peer_addr_type_);

    // Reduce the per-attempt connect timeout from the 30 s default so a
    // resource-conflict failure surfaces quickly instead of blocking the radio
    // task for the full default timeout. See
    // docs/BugReport_CYD_RPS_v0.1.6.md §8.6.
    cli->setConnectTimeout(kConnectTimeoutSeconds);

    // The HOST may not yet have finished role resolution when the JOIN first
    // tries to connect. Retry a bounded number of times with a short backoff
    // instead of giving up on the first connect() failure. See
    // docs/BugReport_CYD_RPS_v0.1.5.md §6.2 and §8.2.
    bool connected = false;
    for (int attempt = 0; attempt < kConnectRetries; ++attempt) {
        if (attempt > 0) {
            vTaskDelay(pdMS_TO_TICKS(kConnectRetryDelayMs));
        }
        if (cli->connect(addr)) {
            connected = true;
            break;
        }
        Serial.printf("BLE: connect attempt %d/%d failed\n", attempt + 1, kConnectRetries);
    }

    if (!connected) {
        Serial.println("ERROR: BLE join connection failed after retries");
        // Restart advertising so this unit remains discoverable if the state
        // machine re-enters a discovery/negotiation cycle. See
        // docs/BugReport_CYD_RPS_v0.1.6.md §8.6.
        start_advertising();
        NimBLEDevice::deleteClient(cli);
        client_ = nullptr;
        sm_post_event(Event::EV_ERROR);
        return;
    }

    // Discover the remote move characteristic and subscribe to notifications.
    NimBLERemoteService* remote_service = cli->getService(RPS_SERVICE_UUID_STR);
    if (!remote_service) {
        sm_post_event(Event::EV_ERROR);
        return;
    }
    remote_char_ = remote_service->getCharacteristic(RPS_MOVE_CHAR_UUID_STR);
    if (!remote_char_) {
        sm_post_event(Event::EV_ERROR);
        return;
    }

    if (!remote_char_ptr(remote_char_)->subscribe(
            true,
            [](NimBLERemoteCharacteristic* /*pChar*/, uint8_t* pData, size_t length, bool /*isNotify*/) {
                if (pData && length > 0) {
                    ble_service().on_move_received(static_cast<Move>(pData[0]));
                }
            })) {
        sm_post_event(Event::EV_ERROR);
        return;
    }

    // The join-side link is fully usable now; announce the connection.
    // (The onConnect callback is suppressed for JOIN until remote_char_ is set
    // so that EV_CONNECTED is only posted after service discovery.)
    connected_ = true;
    sm_post_event(Event::EV_CONNECTED);
}

// ---------------------------------------------------------------------------
// Connection / move callbacks
// ---------------------------------------------------------------------------

void BleService::on_connected() {
    connected_ = true;

    // For the JOIN role we wait until service discovery and subscription are
    // complete before posting EV_CONNECTED; that keeps the PUML Connecting
    // state aligned with a usable link. For the HOST role the link is usable
    // as soon as a central connects.
    if (role_ == Role::HOST || (role_ == Role::JOIN && remote_char_ != nullptr)) {
        sm_post_event(Event::EV_CONNECTED);
    }
}

void BleService::on_disconnected() {
    connected_ = false;
    role_ = Role::NONE;
    remote_char_ = nullptr;
    client_ = nullptr;
    sm_post_event(Event::EV_DISCONNECTED);
}

void BleService::on_move_received(Move move) {
    if (move == Move::NONE) {
        return;
    }
    pending_peer_move_ = move;
    sm_post_event(Event::EV_PEER_MOVE_RECEIVED);
}

// ---------------------------------------------------------------------------
// Move transmission
// ---------------------------------------------------------------------------

bool BleService::send_move(Move move) {
    if (move == Move::NONE) {
        return false;
    }
    uint8_t byte = static_cast<uint8_t>(move);

    if (role_ == Role::HOST) {
        // Peripheral: update local characteristic and notify the central.
        if (!move_char_) {
            return false;
        }
        NimBLECharacteristic* ch = char_ptr(move_char_);
        ch->setValue(&byte, 1);
        ch->notify();
        return true;
    } else if (role_ == Role::JOIN) {
        // Central: write directly to the peripheral characteristic.
        if (!remote_char_) {
            return false;
        }
        return remote_char_ptr(remote_char_)->writeValue(&byte, 1, false);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Dedicated radio task
// ---------------------------------------------------------------------------

void BleService::radio_task_entry(void* param) {
    static_cast<BleService*>(param)->radio_task_loop();
    vTaskDelete(nullptr);
}

void BleService::radio_task_loop() {
    while (true) {
        RadioCommand cmd;
        if (xQueueReceive(radio_cmd_queue_, &cmd, portMAX_DELAY) == pdPASS) {
            if (cmd == RadioCommand::START_DISCOVERY) {
                do_start_discovery();
                // Kick off deferred advertising immediately while still running
                // on the dedicated task stack.
                start_advertising_if_needed();
            } else if (cmd == RadioCommand::STOP_DISCOVERY) {
                do_stop_discovery();
            } else if (cmd == RadioCommand::BECOME_HOST) {
                // Resolve role (HOST/JOIN) and configure the radio accordingly.
                // The actual NimBLE reconfiguration happens inside resolve_role(),
                // become_host(), and become_join(), all running on this task stack.
                resolve_role();
            } else if (cmd == RadioCommand::CONNECT_TO_HOST) {
                // Perform the full JOIN-side connection handshake on the dedicated
                // task stack, including GATT service/characteristic discovery and
                // subscription.
                connect_to_host();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Non-blocking periodic update (main task)
// ---------------------------------------------------------------------------

void BleService::update(uint32_t now_ms) {
    // Discovery timeout only. All radio startup/advertising work has moved to
    // the dedicated radio task to keep NimBLE off the Arduino loop stack.
    post_peer_timeout_if_needed(now_ms);
}

void BleService::post_peer_timeout_if_needed(uint32_t now_ms) {
    if (!discovering_ || peer_timeout_posted_ || role_ != Role::NONE) {
        return;
    }
    if (now_ms - discovery_start_ms_ >= BleService::DISCOVERY_TIMEOUT_MS) {
        peer_timeout_posted_ = true;
        do_stop_discovery();
        sm_post_event(Event::EV_PEER_TIMEOUT);
    }
}

uint8_t BleService::discovery_remaining_seconds() const {
    if (!discovering_ && !peer_timeout_posted_) {
        return 0xFF;
    }
    uint32_t elapsed = millis() - discovery_start_ms_;
    if (elapsed >= BleService::DISCOVERY_TIMEOUT_MS) {
        return 0;
    }
    return static_cast<uint8_t>((BleService::DISCOVERY_TIMEOUT_MS - elapsed + 999) / 1000);
}

void BleService::start_advertising_if_needed() {
    if (!advertising_pending_ || !discovering_ || role_ != Role::NONE) {
        return;
    }
    advertising_pending_ = false;
    Serial.printf("BLE: start_advertising_if_needed heap=%u\n", ESP.getFreeHeap());
    if (!start_advertising()) {
        Serial.println("ERROR: BLE advertise start failed");
        do_stop_discovery();
        discovering_ = false;
        sm_post_event(Event::EV_ERROR);
    }
}

bool BleService::start_scanning() {
    if (!scan_) {
        scan_ = NimBLEDevice::getScan();
        NimBLEScan* scan = scan_ptr(scan_);
        if (!scan) {
            return false;
        }
        scan->setAdvertisedDeviceCallbacks(new RpsAdvertisedDeviceCallbacks());
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(50);
    }
    return scan_ptr(scan_)->start(0, nullptr, false);
}

bool BleService::start_advertising() {
    Serial.printf("BLE: start_advertising heap=%u\n", ESP.getFreeHeap());

    if (ESP.getFreeHeap() < MIN_HEAP_FOR_BLE_ADV) {
        Serial.println("ERROR: insufficient heap for advertising");
        return false;
    }

    if (!advertising_) {
        advertising_ = NimBLEDevice::getAdvertising();
        NimBLEAdvertising* adv = adv_ptr(advertising_);
        if (!adv) {
            return false;
        }
        adv->addServiceUUID(RPS_SERVICE_UUID_STR);

        // Embed the local public MAC in the manufacturer-specific data so the
        // peer can resolve roles using a stable, symmetric identifier instead
        // of the random advertisement address. See
        // docs/BugReport_CYD_RPS_v0.1.5.md §6.3 and §8.3.
        std::vector<uint8_t> manu;
        manu.push_back(static_cast<uint8_t>(kManufacturerCompanyId & 0xFF));
        manu.push_back(static_cast<uint8_t>((kManufacturerCompanyId >> 8) & 0xFF));
        const uint8_t* local = NimBLEDevice::getAddress().getNative();
        manu.insert(manu.end(), local, local + 6);
        adv->setManufacturerData(manu);
    }
    NimBLEAdvertising* adv = adv_ptr(advertising_);
    if (adv->isAdvertising()) {
        return true;
    }
    return adv->start();
}

void BleService::stop_scanning() {
    if (scan_) {
        NimBLEScan* scan = scan_ptr(scan_);
        if (scan->isScanning()) {
            scan->stop();
        }
    }
}

void BleService::stop_advertising() {
    if (advertising_) {
        NimBLEAdvertising* adv = adv_ptr(advertising_);
        if (adv->isAdvertising()) {
            adv->stop();
        }
    }
}

} // namespace app
