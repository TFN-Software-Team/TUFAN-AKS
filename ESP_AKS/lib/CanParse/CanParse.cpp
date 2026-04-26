#include "CanParse.h"

namespace CanParse {

bool parseMotorStatus(const twai_message_t& msg, MotorStatus& out) {
    if (msg.data_length_code < 4)
        return false;

    out.rpm = static_cast<uint16_t>((msg.data[0] << 8) | msg.data[1]);
    out.torqueFeedback =
        static_cast<int16_t>((msg.data[2] << 8) | msg.data[3]);
    out.errorFlags = (msg.data_length_code >= 5) ? msg.data[4] : 0;
    out.isValid = true;
    return true;
}

bool parseBmsConfig(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 6)
        return false;

    out.TEL_bmsPackVoltageDeciV =
        static_cast<uint16_t>((msg.data[2] << 8) | msg.data[3]);
    out.TEL_bmsAverageCellVoltageMv =
        static_cast<uint16_t>((msg.data[4] << 8) | msg.data[5]);
    out.TEL_bmsDataValid = true;
    return true;
}

bool parseBmsLive(const twai_message_t& msg, TelemetryData& out) {
    if (msg.data_length_code < 6)
        return false;

    out.TEL_bmsErrorFlags = msg.data[0];
    out.TEL_bmsCurrentDeciA = static_cast<int8_t>(msg.data[1]);
    out.TEL_bmsTemperatureC = static_cast<int16_t>(msg.data[3]) - 100;
    out.TEL_bmsSoc = msg.data[5];
    out.TEL_bmsDataValid = true;
    return true;
}

bool isMotorStatusTimedOut(bool hasSeen,
                           bool lastValid,
                           TickType_t now,
                           TickType_t lastTick,
                           TickType_t timeoutTicks) {
    if (!hasSeen || !lastValid)
        return false;
    return static_cast<TickType_t>(now - lastTick) >= timeoutTicks;
}

}  // namespace CanParse
