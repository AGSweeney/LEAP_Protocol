# MOD54417 — LeapGateway-Embedded

Upcoming NetBurner port of **LeapGateway-Embedded**: LEAP cell controller, EtherNet/IP
bridge, and HTTP commissioning UI on a MOD54417 module.

## Status

**Planned** — this folder is reserved for the NNDK gateway project. No firmware here yet.

| Piece | Reference |
| --- | --- |
| Gateway application (HTTP, session hub, PD I/O, bridge config) | [`../../../x86-32/D945GSEJT/LeapGateway/`](../../../x86-32/D945GSEJT/LeapGateway/) |
| NetBurner LEAP transport / NNDK layout | [`../../MOD54415LC/LeapPort/`](../../MOD54415LC/LeapPort/) |
| Wire + bridge logic | [`../../../../leap_core/`](../../../../leap_core/) |

## Role (target)

| Side | Protocol |
| --- | --- |
| Plant / PLC | EtherNet/IP (OpENer) |
| Cell | LEAP controller (DISC → MGMT → PD), raw L2 EtherType `0x88B6` |
| Commissioning | HTTP Web UI (`web/index.html` embedded in firmware) |

## Build (when the project lands)

1. Open the gateway NNDK project under this directory in NetBurner Studio.
2. Clean + Build `Release`.
3. Flash the MOD54417 target board.

Build workflow will match the existing device port — see
[`../../MOD54415LC/README.md`](../../MOD54415LC/README.md).
