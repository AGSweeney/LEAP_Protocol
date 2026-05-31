# `tests`

Protocol conformance and regression tests.

## Test Categories

| Category | Description |
| --- | --- |
| CRC verification | Validate CRC-16/XMODEM and CRC-32C against published check values and golden vectors |
| Header validation | Accept valid frames; reject malformed lengths, bad magic, unsupported version |
| Payload rejection | Reject bad-CRC, truncated, and stale-sequence frames without side effects |
| Session/state | Owner-lease enforcement, REBOOT_RECOVERY, state-machine transitions |
| Padding boundary | Minimum transmitted Ethernet payload (50 bytes); CRC/length independence from padding |
