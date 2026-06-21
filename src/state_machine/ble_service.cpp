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
        if (!advertisedDevice->isAdvertisingService(service_uuid)) {
            return;
        }

        // The Host advertises on its public MAC. Prefer the advertised address
        // when it is public; otherwise fall back to the public MAC carried in
        // the manufacturer-specific data. This satisfies Decision D08: connect
        // to the Host public MAC directly with BLE_ADDR_PUBLIC instead of
        // reconstructing an address from getNative() with the wrong type.
        NimBLEAddress addr = advertisedDevice->getAddress();
        uint8_t public_mac[6];
        bool public_mac_valid = false;

        if (addr.getType() == BLE_ADDR_PUBLIC) {
            memcpy(public_mac, addr.getNative(), 6);
            public_mac_valid = true;
        } else {
            const std::string manu = advertisedDevice->getManufacturerData();
            if (manu.length() >= 12 &&
                static_cast<uint8_t>(manu[0]) == (BleService::PRESENCE_COMPANY_ID & 0xFF) &&
                static_cast<uint8_t>(manu[1]) == ((BleService::PRESENCE_COMPANY_ID >> 8) & 0xFF)) {
                memcpy(public_mac, manu.data() + 2 + 4, 6);  // after 4-char device ID
                public_mac_valid = true;
            }
        }

        if (public_mac_valid) {
            ble_service().on_host_found(public_mac);
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
// Initialization
// ---------------------------------------------------------------------------

bool BleService::init() {
    if (initialized_) {
        return true;
    }

#if defined(WOKWI_SIMULATION)
    // Wokwi's ESP32 emulator does not emulate NimBLE reliably. Treat BLE init
    // as success so the boot flow reaches SCR_Start; multiplayer is unavailable
    // in simulation. LL-037.
    Serial.println("CYD_RPS: Wokwi simulation - BLE stack skipped");
    initialized_ = true;
    return true;
#else
    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Use the public MAC for Host advertising and for the stable device ID.
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

    // Capture our public MAC for beacon/device-id generation.
    NimBLEAddress local_addr = NimBLEDevice::getAddress();
    memcpy(public_mac_, local_addr.getNative(), 6);

    // Create the GATT server and move characteristic up-front so both roles
    // share the same attribute handle layout. LL-044: start the GATT server
    // before any GAP scan/advertise/connection procedure is active.
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

    srv->start();

    initialized_ = true;
    return true;
#endif
}

void BleService::start_radio_task() {
    if (radio_task_handle_ != nullptr) {
        return;
    }
    if (radio_cmd_queue_ == nullptr) {
        radio_cmd_queue_ = xQueueCreate(8, sizeof(RadioCommand));
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

// ---------------------------------------------------------------------------
// Public API wrappers around the radio task or direct path
// ---------------------------------------------------------------------------

void BleService::start_presence_beacon() {
    if (radio_task_ready()) {
        RadioCommand cmd = {RadioCommandType::START_PRESENCE, {0}};
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_start_presence_beacon();
}

void BleService::stop_presence_beacon() {
    if (radio_task_ready()) {
        RadioCommand cmd = {RadioCommandType::STOP_PRESENCE, {0}};
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_stop_presence_beacon();
}

void BleService::stop_host_advertising() {
    if (radio_task_ready()) {
        RadioCommand cmd = {RadioCommandType::STOP_HOST, {0}};
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_stop_host_advertising();
}

void BleService::restart_host_advertising() {
    if (radio_task_ready()) {
        RadioCommand cmd = {RadioCommandType::RESTART_HOST, {0}};
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_restart_host_advertising();
}

void BleService::become_host() {
    if (radio_task_ready()) {
        RadioCommand cmd = {RadioCommandType::BECOME_HOST, {0}};
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_become_host();
}

void BleService::connect_to_host(const uint8_t host_public_mac[6]) {
    if (radio_task_ready()) {
        RadioCommand cmd;
        cmd.type = RadioCommandType::CONNECT_TO_HOST;
        memcpy(cmd.mac, host_public_mac, 6);
        xQueueSend(radio_cmd_queue_, &cmd, 0);
        return;
    }
    do_connect_to_host(host_public_mac);
}

void BleService::signal_start_presence_beacon() { start_presence_beacon(); }
void BleService::signal_stop_presence_beacon() { stop_presence_beacon(); }
void BleService::signal_stop_host_advertising() { stop_host_advertising(); }
void BleService::signal_restart_host_advertising() { restart_host_advertising(); }
void BleService::signal_become_host() { become_host(); }
void BleService::signal_connect_to_host(const uint8_t host_public_mac[6]) { connect_to_host(host_public_mac); }

// ---------------------------------------------------------------------------
// Direct implementation: presence beacon
// ---------------------------------------------------------------------------

void BleService::do_start_presence_beacon() {
    if (!initialized_) {
        return;
    }

    // Stop any active host advertising first.
    do_stop_host_advertising();

    role_ = Role::NONE;
    presence_active_ = true;
    host_active_ = false;
    peer_public_mac_valid_ = false;
    memset(peer_public_mac_, 0, sizeof(peer_public_mac_));

    if (!start_scanning()) {
        Serial.println("ERROR: BLE scan start failed");
        do_stop_presence_beacon();
        sm_post_event(Event::EV_ERROR);
        return;
    }

    start_presence_advertising();
}

void BleService::do_stop_presence_beacon() {
    stop_scanning();
    stop_advertising();
    presence_active_ = false;
}

void BleService::start_presence_advertising() {
    if (!initialized_ || !presence_active_) {
        return;
    }

    if (ESP.getFreeHeap() < MIN_HEAP_FOR_BLE_ADV) {
        Serial.println("ERROR: insufficient heap for presence advertising");
        do_stop_presence_beacon();
        sm_post_event(Event::EV_ERROR);
        return;
    }

    if (!advertising_) {
        advertising_ = NimBLEDevice::getAdvertising();
    }
    NimBLEAdvertising* adv = adv_ptr(advertising_);
    if (!adv) {
        return;
    }

    // Build manufacturer-specific data: company ID 0xFF + 4-char device ID +
    // 6-byte public MAC.
    char id[5];
    get_device_id(id, sizeof(id));

    std::string manu;
    manu.push_back(static_cast<uint8_t>(PRESENCE_COMPANY_ID & 0xFF));
    manu.push_back(static_cast<uint8_t>((PRESENCE_COMPANY_ID >> 8) & 0xFF));
    manu.append(id);
    manu.append(reinterpret_cast<const char*>(public_mac_), 6);

    adv->setManufacturerData(manu);
    adv->setMinInterval(PRESENCE_ADV_MIN_INTERVAL);
    adv->setMaxInterval(PRESENCE_ADV_MAX_INTERVAL);
    adv->start();
}

// ---------------------------------------------------------------------------
// Direct implementation: Host mode
// ---------------------------------------------------------------------------

void BleService::do_become_host() {
    if (!initialized_) {
        sm_post_event(Event::EV_ERROR);
        return;
    }

    role_ = Role::HOST;
    presence_active_ = false;
    host_active_ = true;
    peer_public_mac_valid_ = false;
    memset(peer_public_mac_, 0, sizeof(peer_public_mac_));

    // Stop the presence beacon/scan to enter a clean peripheral-only state.
    stop_scanning();
    stop_advertising();

    // Guard delay: let the controller leave scan/advertise state before
    // restarting connectable advertising. v0.2.0: 20 ms guard after stopping
    // scan.
    vTaskDelay(pdMS_TO_TICKS(HOST_STOP_SCAN_TO_ADV_GUARD_MS));

    start_host_advertising();

    // Continue scanning so we can detect a simultaneous peer Host and resolve
    // the conflict by becoming Join (see definition.md conflict resolution).
    start_scanning();

    host_wait_start_ms_ = millis();
    host_wait_timeout_posted_ = false;
}

void BleService::start_host_advertising() {
    if (!initialized_) {
        return;
    }

    if (ESP.getFreeHeap() < MIN_HEAP_FOR_BLE_ADV) {
        Serial.println("ERROR: insufficient heap for host advertising");
        do_stop_host_advertising();
        sm_post_event(Event::EV_ERROR);
        return;
    }

    if (!advertising_) {
        advertising_ = NimBLEDevice::getAdvertising();
    }
    NimBLEAdvertising* adv = adv_ptr(advertising_);
    if (!adv) {
        return;
    }

    adv->addServiceUUID(RPS_SERVICE_UUID_STR);

    // Continue to include the public MAC in manufacturer data so a Joiner can
    // fall back to it if its controller reports a non-public address type.
    char id[5];
    get_device_id(id, sizeof(id));
    std::string manu;
    manu.push_back(static_cast<uint8_t>(PRESENCE_COMPANY_ID & 0xFF));
    manu.push_back(static_cast<uint8_t>((PRESENCE_COMPANY_ID >> 8) & 0xFF));
    manu.append(id);
    manu.append(reinterpret_cast<const char*>(public_mac_), 6);
    adv->setManufacturerData(manu);

    adv->setMinInterval(HOST_ADV_MIN_INTERVAL);
    adv->setMaxInterval(HOST_ADV_MAX_INTERVAL);
    adv->start();
}

void BleService::do_stop_host_advertising() {
    stop_advertising();
    stop_scanning();
    host_active_ = false;
    host_wait_timeout_posted_ = false;
}

void BleService::do_restart_host_advertising() {
    do_stop_host_advertising();
    vTaskDelay(pdMS_TO_TICKS(HOST_STOP_SCAN_TO_ADV_GUARD_MS));
    role_ = Role::HOST;
    host_active_ = true;
    start_host_advertising();
    host_wait_start_ms_ = millis();
    host_wait_timeout_posted_ = false;
}

// ---------------------------------------------------------------------------
// Direct implementation: Join mode
// ---------------------------------------------------------------------------

void BleService::do_connect_to_host(const uint8_t host_public_mac[6]) {
    if (!initialized_ || !host_public_mac) {
        sm_post_event(Event::EV_ERROR);
        return;
    }

    role_ = Role::JOIN;
    presence_active_ = false;
    host_active_ = false;

    // Stop our own beacon/scan before connecting as a central.
    stop_scanning();
    stop_advertising();

    // Capture the peer public MAC for device-id display and fallback.
    memcpy(peer_public_mac_, host_public_mac, 6);
    peer_public_mac_valid_ = true;

    NimBLEClient* cli = NimBLEDevice::createClient();
    if (!cli) {
        sm_post_event(Event::EV_CONNECT_FAILED);
        return;
    }
    cli->setClientCallbacks(new RpsClientCallbacks());
    cli->setConnectTimeout(JOIN_CONNECT_TIMEOUT_SEC);

    // Construct the Host address with BLE_ADDR_PUBLIC directly. Decision D08:
    // do not reconstruct the address from getNative() with the wrong type.
    uint8_t host_mac[6];
    memcpy(host_mac, host_public_mac, 6);
    NimBLEAddress addr(host_mac, BLE_ADDR_PUBLIC);

    bool connected = false;
    for (int attempt = 0; attempt < GameContext::kMaxJoinRetries; ++attempt) {
        // Guard delay after stopping advertising before connect(). v0.2.0: 50 ms.
        vTaskDelay(pdMS_TO_TICKS(STOP_ADV_TO_CONNECT_GUARD_MS));

        Serial.printf("JOIN: connecting to %s (attempt %d/%d)\n",
                      addr.toString().c_str(), attempt + 1, GameContext::kMaxJoinRetries);

        if (cli->connect(addr)) {
            connected = true;
            break;
        }

        Serial.printf("JOIN: connect attempt %d/%d failed\n", attempt + 1, GameContext::kMaxJoinRetries);
    }

    if (!connected) {
        Serial.println("ERROR: BLE join connection failed after retries");
        NimBLEDevice::deleteClient(cli);
        client_ = nullptr;
        sm_post_event(Event::EV_CONNECT_FAILED);
        return;
    }

    client_ = cli;

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

    connected_ = true;
    sm_post_event(Event::EV_CONNECTED);
}

// ---------------------------------------------------------------------------
// Discovery callback
// ---------------------------------------------------------------------------

void BleService::on_host_found(const uint8_t host_public_mac[6]) {
    if (!host_public_mac) {
        return;
    }

    // Self-discovery guard.
    if (memcmp(public_mac_, host_public_mac, 6) == 0) {
        return;
    }

    AppStateMachine& sm = sm_instance();

    if (role_ == Role::HOST && host_active_) {
        // We are hosting and see a peer Host advertisement. The device with the
        // lower public MAC remains Host; the higher becomes Join.
        if (memcmp(public_mac_, host_public_mac, 6) < 0) {
            // Local MAC is lower: remain Host.
            return;
        }
        // Local MAC is higher: become Join.
        memcpy(peer_public_mac_, host_public_mac, 6);
        peer_public_mac_valid_ = true;
        memcpy(sm.ctx().host_mac, host_public_mac, 6);
        sm.ctx().host_mac_valid = true;
        sm.ctx().peer_host_seen = true;
        sm_post_event(Event::EV_HOST_FOUND);
        return;
    }

    // Normal Start-screen auto-join path.
    memcpy(peer_public_mac_, host_public_mac, 6);
    peer_public_mac_valid_ = true;
    memcpy(sm.ctx().host_mac, host_public_mac, 6);
    sm.ctx().host_mac_valid = true;
    sm_post_event(Event::EV_HOST_FOUND);
}

// ---------------------------------------------------------------------------
// Connection / move callbacks
// ---------------------------------------------------------------------------

void BleService::on_connected() {
    connected_ = true;
    if (role_ == Role::HOST) {
        // For the Host, the link is usable as soon as a central connects.
        sm_post_event(Event::EV_CONNECTED);
    }
    // For the JOIN role we wait until service discovery and subscription are
    // complete before posting EV_CONNECTED; that keeps the Joining state aligned
    // with a usable link. See do_connect_to_host().
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
        if (!move_char_) {
            return false;
        }
        NimBLECharacteristic* ch = char_ptr(move_char_);
        ch->setValue(&byte, 1);
        ch->notify();
        return true;
    } else if (role_ == Role::JOIN) {
        if (!remote_char_) {
            return false;
        }
        return remote_char_ptr(remote_char_)->writeValue(&byte, 1, false);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Disconnect
// ---------------------------------------------------------------------------

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
            switch (cmd.type) {
                case RadioCommandType::START_PRESENCE:
                    do_start_presence_beacon();
                    break;
                case RadioCommandType::STOP_PRESENCE:
                    do_stop_presence_beacon();
                    break;
                case RadioCommandType::STOP_HOST:
                    do_stop_host_advertising();
                    break;
                case RadioCommandType::RESTART_HOST:
                    do_restart_host_advertising();
                    break;
                case RadioCommandType::BECOME_HOST:
                    do_become_host();
                    break;
                case RadioCommandType::CONNECT_TO_HOST:
                    do_connect_to_host(cmd.mac);
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Non-blocking periodic update (main task)
// ---------------------------------------------------------------------------

void BleService::update(uint32_t /*now_ms*/) {
    // Host timeout event posting is driven by AppStateMachine::update() so the
    // state machine remains the single owner of event dispatch timing. This
    // hook is reserved for future non-blocking BLE housekeeping.
}

void BleService::post_host_timeout_if_needed(uint32_t now_ms) {
    if (!host_active_ || host_wait_timeout_posted_) {
        return;
    }
    if (now_ms - host_wait_start_ms_ >= HOST_WAIT_TIMEOUT_MS) {
        host_wait_timeout_posted_ = true;
        sm_post_event(Event::EV_HOST_TIMEOUT);
    }
}

uint8_t BleService::host_wait_percent_remaining() const {
    if (!host_active_ || host_wait_timeout_posted_) {
        return 0;
    }
    uint32_t elapsed = millis() - host_wait_start_ms_;
    if (elapsed >= HOST_WAIT_TIMEOUT_MS) {
        return 0;
    }
    return static_cast<uint8_t>(((HOST_WAIT_TIMEOUT_MS - elapsed) * 100) / HOST_WAIT_TIMEOUT_MS);
}

bool BleService::host_wait_timed_out() const {
    if (!host_active_ || host_wait_timeout_posted_) {
        return false;
    }
    return (millis() - host_wait_start_ms_) >= HOST_WAIT_TIMEOUT_MS;
}

void BleService::mark_host_wait_timeout_posted() {
    host_wait_timeout_posted_ = true;
}

// ---------------------------------------------------------------------------
// Device ID helpers
// ---------------------------------------------------------------------------

void BleService::get_device_id(char* out, size_t len) const {
    if (!out || len < 5) {
        return;
    }
    // Last two bytes of the public MAC rendered as four uppercase hex digits.
    snprintf(out, len, "%02X%02X", public_mac_[4], public_mac_[5]);
}

void BleService::get_peer_device_id(char* out, size_t len) const {
    if (!out || len < 5) {
        return;
    }
    if (!peer_public_mac_valid_) {
        out[0] = '\0';
        return;
    }
    snprintf(out, len, "%02X%02X", peer_public_mac_[4], peer_public_mac_[5]);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool BleService::start_scanning() {
    if (!initialized_) {
        return false;
    }
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
        // Clear the configured data so the next mode starts from a clean slate.
        adv->removeServiceUUID(NimBLEUUID(RPS_SERVICE_UUID_STR));
        adv->setManufacturerData(std::string());
    }
}

} // namespace app
