# NetBurner

LEAP embedded device ports for NetBurner hardware.

| Target | Path |
| --- | --- |
| MOD54415LC | [MOD54415LC/](MOD54415LC/) |

## Current status

- `MOD54415LC` port is active and passes `device_conformance` in Studio.
- Cyclic PD WRITE and EXCHANGE are validated at bench rates used in conformance.
- Build/flash workflow remains NetBurner-native (NNDK project build), not root CMake.
