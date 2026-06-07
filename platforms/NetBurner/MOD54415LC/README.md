# MOD54415LC

LEAP device port for NetBurner MOD54415LC.

## Status

- Port is active.
- Passes full `device_conformance` scenario in LEAP Conformance Studio.
- Cyclic PD WRITE and cyclic PD EXCHANGE are validated.

## Build and flash

Build this firmware from the NetBurner project (`LeapPort`) in NNDK:

1. Clean + Build `Release`
2. Flash the target board
3. Re-run Studio conformance / I-O bench

## Notes

- LEAP transport is raw L2 (EtherType), no IP required for protocol traffic.
- Studio I/O bench supports optional DIAG polling during soak via checkbox.
- If DIAG polling is enabled at very aggressive cycle periods, bench jitter can
  increase due to additional diagnostics traffic.
