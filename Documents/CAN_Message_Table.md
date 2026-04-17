# TUFAN-AKS — CAN Message Table

**Bus Speed:** 500 kbps  
**Byte Order:** Big-endian (MSB first)

---

## 0x100 — Torque Command

| Field | Direction | AKS → Motor Driver |
|-------|-----------|---------------------|
| **Period** | | 10 ms (100 Hz) |
| **DLC** | | 2 bytes |

| Byte | Signal | Type | Unit | Range | Description |
|------|--------|------|------|-------|-------------|
| 0 | TORQUE_CMD_H | uint8 | — | — | Torque command high byte |
| 1 | TORQUE_CMD_L | uint8 | — | — | Torque command low byte |

**Torque value** = `(Byte[0] << 8) | Byte[1]`

> **Note:** When VCU state ≠ DRIVE, AKS sends torque = 0 for safety.

---

## 0x200 — Motor Status

| Field | Direction | Motor Driver → AKS |
|-------|-----------|---------------------|
| **Period** | | TBD (set by motor driver) |
| **DLC** | | 5 bytes (minimum 4) |

| Byte | Signal | Type | Unit | Range | Description |
|------|--------|------|------|-------|-------------|
| 0 | RPM_H | uint8 | — | — | Motor RPM high byte |
| 1 | RPM_L | uint8 | — | — | Motor RPM low byte |
| 2 | TORQUE_FB_H | int8 | — | — | Torque feedback high byte |
| 3 | TORQUE_FB_L | int8 | — | — | Torque feedback low byte |
| 4 | ERROR_FLAGS | uint8 | — | 0x00–0xFF | Motor driver error flags |

**RPM** = `(Byte[0] << 8) | Byte[1]`  
**Torque feedback** = `(int16_t)((Byte[2] << 8) | Byte[3])`  
**Error flags** = `Byte[4]` — any non-zero value triggers `FAULT_DETECTED` event in VCU.

> [!WARNING]
> **TODO:** Byte layout is a placeholder. Confirm exact format with motor driver team before first CAN test.

### Error Flag Bits (Placeholder)

| Bit | Flag | Description |
|-----|------|-------------|
| 0 | OVER_TEMP | Motor over-temperature |
| 1 | OVER_CURRENT | Phase current limit exceeded |
| 2 | OVER_VOLTAGE | DC bus over-voltage |
| 3 | UNDER_VOLTAGE | DC bus under-voltage |
| 4 | ENCODER_FAULT | Encoder signal lost |
| 5–7 | RESERVED | — |

> **TODO:** Confirm error flag definitions with motor driver team.

---

## 0x300 — BMS Status

| Field | Direction | BMS → AKS |
|-------|-----------|-----------|
| **Period** | | Set by BMS |
| **DLC** | | Variable |

> **OUT OF SCOPE** — This message is received and **ignored** by AKS.  
> BMS integration is not in this team's scope.  
> See `CanManager::processRxMessages()` — logged at `ESP_LOGD` level only.

---

## CAN ID Summary

```
0x100  AKS → Motor Driver    Torque Command      10 ms
0x200  Motor Driver → AKS    Motor Status         TBD
0x300  BMS → AKS             BMS Status           IGNORED
```
