# LoRa Link Analysis

This note captures the current AKS-side telemetry bandwidth estimate and the practical assumptions for Phase 3 reliability work.

## Current AKS Telemetry Payload

Current uplink format:

- ASCII CSV line
- Prefix: `TEL,<version>,<sequence>,...`
- Terminator: `\r\n`
- Rate: `5 Hz`

Representative packet:

```text
TEL,1,42,3150,12,0,1,0,77,-15,28,612,3400,0,1\r\n
```

Typical payload size is approximately `45-70 bytes` depending on numeric field widths.
A conservative planning budget of `70 bytes` per packet gives:

- `70 bytes * 5 Hz = 350 bytes/s`
- `350 bytes/s * 10 bits/byte ~= 3500 bit/s` on the UART side including start/stop overhead

## Interpretation

Important distinction:

- The ESP32 <-> E32 UART link is configured for `9600 baud`.
- The radio air data rate of the E32 module may be lower than the UART baud.
- Therefore, final field testing must confirm that the selected E32 radio configuration can drain the UART input fast enough at the chosen packet rate.

## Current Reliability Policy

Implemented now:

- No AKS retransmission
- No AKS-level ACK handling
- Latest-sample-wins behavior
- Sequence counter for loss detection
- AUX gate checked before each TX attempt

Implication:

- Packet loss is observable at UKS by sequence gaps.
- Lost packets are not resent.
- If AUX is busy, AKS skips the current sample and waits for the next period.

## Recommended Field Checks

Before locking Phase 3 complete, validate:

1. Actual E32 air data rate and module configuration in hardware.
2. Whether `5 Hz` remains loss-free during simultaneous RX command traffic.
3. Whether AUX busy events appear frequently under worst-case telemetry load.
4. Whether UKS parser cleanly handles skipped sequence numbers.

## If Link Margin Is Poor

Preferred mitigation order:

1. Reduce telemetry field count.
2. Reduce transmit rate below `5 Hz`.
3. Move from verbose ASCII to compact framed binary payloads.
4. Add selective ACK / retry only if field testing proves it necessary.
