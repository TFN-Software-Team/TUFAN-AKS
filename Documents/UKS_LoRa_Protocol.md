# UKS <-> AKS LoRa Protocol

This document defines the current command and telemetry contract between the UKS ground unit and the AKS firmware.

## Link Assumptions

- Radio module: E32-433T30D
- UART mode: transparent mode
- Selected startup mode: `M0 = 0`, `M1 = 0`
- UART baud: `9600`
- Telemetry transmit period: `200 ms` (`5 Hz`)
- No application-layer ACK or retransmission is implemented in AKS at this stage.

Loss handling policy:

- AKS transmits the latest telemetry snapshot only.
- If a packet is missed, the next packet replaces it.
- UKS should detect packet loss by checking the telemetry sequence counter.

## UKS -> AKS Command Bytes

Single-byte command values currently recognized by AKS:

| Symbol | Value | Meaning |
| --- | --- | --- |
| `UKS_CMD_EMERGENCY_STOP` | `0xA1` | Immediate emergency stop request |
| `UKS_CMD_START` | `0xA2` | Request `IDLE -> READY` |
| `UKS_CMD_STOP` | `0xA3` | Request reset / stop |
| `UKS_CMD_DRIVE_ENABLE` | `0xA4` | Request `READY -> DRIVE` |

Receiver behavior:

- Unknown command bytes are ignored.
- Commands are interpreted from `LO_rxBuffer[0]`.
- AKS does not currently implement command CRC, framing bytes, or retransmission.

## AKS -> UKS Telemetry Format

AKS transmits one ASCII CSV line per sample with `CRLF` line termination.

Example:

```text
TEL,1,42,3150,12,0,1,0,77,-15,28,612,3400,0,1
```

Field order is fixed and must be parsed positionally:

| Index | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `TEL` | literal | Packet type tag |
| 1 | protocol version | `uint8` | Current value: `1` |
| 2 | sequence | `uint32` | Increments on each successful TX |
| 3 | motor RPM | `uint16` | Latest motor RPM |
| 4 | motor torque feedback | `int16` | Latest signed torque feedback |
| 5 | motor error flags | `uint8` | Motor fault flags |
| 6 | motor data valid | `0/1` | `1` if latest motor frame is considered fresh |
| 7 | motor timeout active | `0/1` | `1` if motor status timed out after first reception |
| 8 | BMS SOC | `uint8` | State of charge in percent |
| 9 | BMS current | `int16` | Pack current in deci-amps |
| 10 | BMS temperature | `int16` | Degrees Celsius |
| 11 | BMS pack voltage | `uint16` | Deci-volts |
| 12 | BMS average cell voltage | `uint16` | Millivolts |
| 13 | BMS error flags | `uint8` | BMS fault flags |
| 14 | BMS data valid | `0/1` | `1` if BMS telemetry fields are populated |

UKS parser requirements:

- Reject packets whose field count is not exactly `15`.
- Reject packets whose tag is not `TEL`.
- Track `sequence` to detect dropped packets.
- Treat repeated `sequence` values as duplicate or stale samples.

## Future Extensions

Planned but not implemented:

- Framed command packets with checksum
- ACK / retransmission policy
- Radio configuration management through E32 configuration mode
- Explicit fault and VCU state fields in uplink telemetry
