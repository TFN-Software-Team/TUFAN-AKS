# Contributing

## Branch Naming

All feature and fix branches must follow this format:

`AKS-filenamehere`

Examples:

- `AKS-canmanager-bms-parser`
- `AKS-vculogic-reset-interlock`
- `AKS-readme-update`

Use short, lowercase, hyphen-separated suffixes after the `AKS-` prefix.

## Variable and Function Naming

Project-local identifiers must use a module prefix in uppercase, followed by an underscore and a descriptive name:

`PREFIX_descriptiveName`

Examples:

- `CAN_motorRpm`
- `LO_uartConfig`
- `HMI_currentSpeed`
- `TEL_payloadLength`

Prefix guide:

- `CAN_` for CAN / motor / BMS parsing helpers
- `LO_` for LoRa / uplink / radio helpers
- `HMI_` for display and touch helpers
- `TEL_` for telemetry payload and queue data
- `VCU_` for state-machine-local helpers if added later
- `REL_` for relay-related locals if added later

## General Rules

- Use English for code, comments, logs, and documentation updates in this repository.
- Use `vTaskDelay()` inside FreeRTOS tasks. Do not use `delay()`.
- Prefer ESP-IDF log macros such as `ESP_LOGI`, `ESP_LOGW`, and `ESP_LOGE` for state transitions and failure paths.
- Keep documentation synchronized with code whenever CAN IDs, queue payloads, or control commands change.
