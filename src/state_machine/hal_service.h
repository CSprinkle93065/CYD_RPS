#pragma once

// Hardware abstraction service for runtime probes.
//
// IMPORTANT (LL-034): This module does NOT initialize display, touch, or BLE
// hardware. Initialization is owned by main.cpp / app_init().  HalService only
// records the result of those initialization probes so the generated state
// machine guards can reference them.

#include <cstdint>

namespace app {

class HalService {
public:
    HalService() = default;

    // Record the outcome of the main.cpp initialization sequence.
    void mark_initialized(bool ok);
    void mark_ble_init_failed(bool failed);
    void mark_hw_init_failed(bool failed);
    void mark_fatal(bool fatal);

    // Guards read these at decision points.
    bool init_ok() const { return init_ok_; }
    bool ble_init_failed() const { return ble_init_failed_; }
    bool hw_init_failed() const { return hw_init_failed_; }
    bool fatal() const { return fatal_; }

private:
    bool init_ok_ = false;
    bool ble_init_failed_ = false;
    bool hw_init_failed_ = false;
    bool fatal_ = false;
};

// Global singleton accessor.
HalService& hal_service();

} // namespace app
