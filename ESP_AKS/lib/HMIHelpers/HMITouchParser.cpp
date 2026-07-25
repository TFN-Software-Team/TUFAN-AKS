#include "HMITouchParser.h"

// --- HMI Command Frame Format ---
// The HMI must send commands as a 3-byte frame to ensure integrity:
// [HEADER] [COMMAND] [CHECKSUM]
// HEADER   = 0x5A
// COMMAND  = HMI_CMD_... (e.g. 0x01 for START)
// CHECKSUM = ~COMMAND (bitwise NOT of COMMAND)
// 
// Example START frame: 0x5A 0x01 0xFE

bool HMI_parseTouchByte(uint8_t rxByte, HMI_TouchParserState& state, uint8_t& outCommand) {
    if (rxByte == 0xFF || rxByte == 0x00) {
        // Nextion invalid/ack artıkları - güvenle atla ve state resetle
        state.rxState = 0;
        return false;
    }

    if (state.rxState == 0) {
        if (rxByte == 0x5A) {
            state.rxState = 1;
        }
    } else if (state.rxState == 1) {
        state.pendingCmd = rxByte;
        state.rxState = 2;
    } else if (state.rxState == 2) {
        uint8_t expectedChecksum = (uint8_t)(~state.pendingCmd);
        state.rxState = 0; // reset state for next command
        if (rxByte == expectedChecksum) {
            outCommand = state.pendingCmd;
            return true;
        } else {
            // Checksum mismatch, log is handled outside or can be ignored here for purity.
            // Returning false since we didn't decode a valid command.
            return false;
        }
    }
    return false;
}
