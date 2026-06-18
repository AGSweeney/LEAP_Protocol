# NetBurner

LEAP embedded ports for NetBurner hardware. Firmware builds in **NNDK** inside each
target folder — not in the repo-root CMake trees.

## Device ports

| Target | Path | Status |
| --- | --- | --- |
| MOD54415LC | [MOD54415LC/](MOD54415LC/) | active — LEAP device |

## Gateway ports

| Target | Path | Status |
| --- | --- | --- |
| MOD54417 | [LeapGateway/MOD54417/](LeapGateway/MOD54417/) | planned — LeapGateway-Embedded |

See [LeapGateway/](LeapGateway/) for shared gateway notes and references to
[`x86-32/D945GSEJT/LeapGateway/`](../x86-32/D945GSEJT/LeapGateway/) application sources.

## Current status

- `MOD54415LC` device port is active and passes `device_conformance` in Studio.
- Cyclic PD WRITE and EXCHANGE are validated at bench rates used in conformance.
- `MOD54417` **LeapGateway-Embedded** port is planned under [LeapGateway/MOD54417/](LeapGateway/MOD54417/).
- Build/flash workflow remains NetBurner-native (NNDK project build), not root CMake.
