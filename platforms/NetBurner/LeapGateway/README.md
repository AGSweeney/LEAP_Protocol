# LeapGateway — NetBurner MOD54417

NetBurner foundation for the embedded **LEAP Gateway** on MOD5441X hardware (MOD54415 / MOD54417).

## Status

**In progress** — networking shell is ready in [`MOD54417_Gateway/`](MOD54417_Gateway/). LEAP gateway logic is to be ported from the x86 LeapGateway sources.

| Piece | Location |
| --- | --- |
| Network Web UI + HTTP API (retained from external reference) | [`MOD54417_Gateway/`](MOD54417_Gateway/) |
| Planned LEAP cell controller gateway | [`MOD54417/`](MOD54417/) (reserved) |
| NetBurner LEAP transport / NNDK layout | [`../MOD54415LC/LeapPort/`](../MOD54415LC/LeapPort/) |
| Wire + bridge logic | [`../../../../leap_core/`](../../../../leap_core/) |

## Build

1. Open the **LeapGateway** NBEclipse workspace (this directory).
2. Import **MOD54417_Gateway** if it is not already in the workspace.
3. Clean + Build **Release**.
4. Flash the MOD54417 target board.

See [`MOD54417_Gateway/README.md`](MOD54417_Gateway/README.md) for commissioning,
AutoUpdate first-flash, and API details. Build workflow matches the existing device
port — see [`../MOD54415LC/README.md`](../MOD54415LC/README.md).
