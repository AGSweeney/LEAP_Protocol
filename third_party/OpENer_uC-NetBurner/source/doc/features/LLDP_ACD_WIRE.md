# LLDP and ACD wire modules

IEEE 802.1AB LLDP (CIP 0x109 / 0x10A) and RFC 5227 ACD (TCP/IP attributes 10 / 11) for NetBurner NNDK.

Enable: `.\scripts\build-nb.ps1 -Profile Extended` or `-DOPENER_LLDP=ON -DOPENER_NB_LLDP=ON -DOPENER_NB_ACD=ON`. Scope and NV: [`SCOPE_ACD_NVDATA.md`](SCOPE_ACD_NVDATA.md).

## Source files

| Component | Path |
|-----------|------|
| LLDP wire | `source/src/ports/netburner/opener_nb_lldp.cpp` |
| ACD wire | `source/src/ports/netburner/opener_nb_acd.cpp` |
| LLDP TX (NNDK) | `source/src/ports/netburner/nndk_overload/lldp.cpp` (in `libOPENER_HAL.a`) |
| CIP bridges | `opener_nb_lldp_cip.c`, `opener_nb_acd_cip.c` |
| NV | `opener_nb_nv.cpp` |
| App hooks | `app/opener.c`, `app/networkconfig.c` |

## LLDP

- TX/RX ethertype **0x88CC**; EtherNet/IP OUI **00:12:0F** subtypes 2, 3, 9 in TX TLVs
- RX via guarded `CustomNetDoRX` in [`nbrtos/source/netrx.cpp`](../../src/ports/netburner/nndk_overload/nbrtos/source/netrx.cpp) + `predef-overload.h` in **`libnetburner.a`** ([`porting/README.md`](../porting/README.md))
- Poll: `OpenerNbLldpPoll()` from `opener_process()`
- Class **0x10A** attrs 2/3 report live neighbor max instance and count; instance 1 placeholder when table is empty
- Identity TLVs refresh on TCP/IP GetAttribute via `networkconfig.c`

## ACD

- RFC 5227 state machine in `opener_nb_acd.cpp`; ARP hook on `pArpFunc`
- TCP/IP attrs 10/11 via `opener_nb_acd_cip.c`
- `OpenerNbAcdInit()` may block during probe; poll via `OpenerNbAcdPoll()`
- Notify I/O from `CheckIoConnectionEvent()` → `OpenerNbAcdNotifyIoConnection()`

## Firmware makefile

Edit [`makefile.init.snippet`](../../src/ports/netburner/app/nbeclipse/makefile.init.snippet): set `OPENER_ROOT`, link `libOPENER_HAL.a` only (no extra `lldp.o`).

Integration checklist: [`porting/README.md`](../porting/README.md).
