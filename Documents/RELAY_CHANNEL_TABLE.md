# Relay Channel Table

All relay outputs are driven through the MCP23S17 and are active-low at the hardware level.

Current software meaning:

- The full bank is treated as the positive contactor group.
- `RelayManager::allOn()` closes every defined positive contactor output.
- `RelayManager::allOff()` de-energizes the full bank.

## Channel Map

| Channel Macro | Index | Current Software Role | Physical Load |
| --- | --- | --- | --- |
| `RELAY_CH_POS_0` | 0 | Positive contactor output 0 | TBD during harness validation |
| `RELAY_CH_POS_1` | 1 | Positive contactor output 1 | TBD during harness validation |
| `RELAY_CH_POS_2` | 2 | Positive contactor output 2 | TBD during harness validation |
| `RELAY_CH_POS_3` | 3 | Positive contactor output 3 | TBD during harness validation |
| `RELAY_CH_POS_4` | 4 | Positive contactor output 4 | TBD during harness validation |
| `RELAY_CH_POS_5` | 5 | Positive contactor output 5 | TBD during harness validation |
| `RELAY_CH_POS_6` | 6 | Positive contactor output 6 | TBD during harness validation |
| `RELAY_CH_POS_7` | 7 | Positive contactor output 7 | TBD during harness validation |
| `RELAY_CH_POS_8` | 8 | Positive contactor output 8 | TBD during harness validation |
| `RELAY_CH_POS_9` | 9 | Positive contactor output 9 | TBD during harness validation |

## Note

Do not assign semantic names such as precharge, main positive, charger enable, or inverter enable in code until the electrical drawing confirms the exact output-to-load mapping.
