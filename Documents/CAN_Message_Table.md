# CAN Message Table

This table documents the CAN frames currently implemented or reserved by the AKS firmware.

## Motor Driver Frames

### `0x100` Torque Command

Direction: `AKS -> Motor Driver`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | Torque MSB | `uint8_t` | raw | High byte of torque command |
| 1 | Torque LSB | `uint8_t` | raw | Low byte of torque command |

Notes:

- The CAN task transmits torque at the control loop rate.
- When VCU state is not `DRIVE`, torque is forced to `0` for safety.

### `0x200` Motor Status

Direction: `Motor Driver -> AKS`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | RPM MSB | `uint8_t` | raw | High byte of motor RPM |
| 1 | RPM LSB | `uint8_t` | raw | Low byte of motor RPM |
| 2 | Torque Feedback MSB | `uint8_t` | raw | High byte of signed torque feedback |
| 3 | Torque Feedback LSB | `uint8_t` | raw | Low byte of signed torque feedback |
| 4 | Error Flags | `uint8_t` | bitfield | Motor driver fault / warning flags |

Freshness rule:

- If no valid `0x200` frame is received for `500 ms`, AKS marks motor data invalid.
- A post-reception motor timeout is treated as a critical safety condition outside `IDLE`.

## Lithium Balance BMS Frames

### `0xE000` Config

Direction: `BMS -> AKS`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | Reserved | `uint8_t` | raw | Reverse-engineered frame byte, currently unused by AKS |
| 1 | Reserved | `uint8_t` | raw | Reverse-engineered frame byte, currently unused by AKS |
| 2 | Pack Voltage MSB | `uint8_t` | x0.1 V | High byte of pack voltage |
| 3 | Pack Voltage LSB | `uint8_t` | x0.1 V | Low byte of pack voltage |
| 4 | Average Cell Voltage MSB | `uint8_t` | x1 mV | High byte of average cell voltage |
| 5 | Average Cell Voltage LSB | `uint8_t` | x1 mV | Low byte of average cell voltage |

### `0xE001` Live

Direction: `BMS -> AKS`

| Byte | Field | Type | Scale | Description |
| --- | --- | --- | --- | --- |
| 0 | Error Flags | `uint8_t` | bitfield | Current firmware assumption for BMS fault flags |
| 1 | Current | `int8_t` | x0.1 A | Signed pack current |
| 2 | Reserved | `uint8_t` | raw | Reverse-engineered frame byte, currently unused by AKS |
| 3 | Temperature | `uint8_t` | `value - 100` | BMS temperature in degree Celsius |
| 4 | Reserved | `uint8_t` | raw | Reverse-engineered frame byte, currently unused by AKS |
| 5 | State of Charge | `uint8_t` | percent | Pack SOC |

Safety notes:

- Nonzero BMS error flags trigger `FAULT_DETECTED`.
- Warning and critical threshold checks are also evaluated in `VcuLogic` using the latest telemetry snapshot.

## Legacy / Reserved

### `0x300` Legacy BMS Status

Direction: `BMS -> AKS`

Status: reserved for backward compatibility logging only. The current firmware does not parse payload fields from this frame.
