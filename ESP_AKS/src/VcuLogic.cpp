#include "VcuLogic.h"
#include "SystemConfig.h"
#include "esp_log.h"

static constexpr const char* TAG = "VCU_LOGIC";

namespace VcuLogic {

void run() {
    ESP_LOGD(TAG, "VCU state machine step");
    // TODO: implement pre-charge, contactor logic, and drive state transitions
}

}  // namespace VcuLogic
