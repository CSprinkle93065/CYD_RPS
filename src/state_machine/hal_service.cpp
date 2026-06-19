#include "hal_service.h"

namespace app {

static HalService s_hal_service;

HalService& hal_service() {
    return s_hal_service;
}

void HalService::mark_initialized(bool ok) {
    init_ok_ = ok;
}

void HalService::mark_ble_init_failed(bool failed) {
    ble_init_failed_ = failed;
}

void HalService::mark_hw_init_failed(bool failed) {
    hw_init_failed_ = failed;
}

void HalService::mark_fatal(bool fatal) {
    fatal_ = fatal;
}

} // namespace app
