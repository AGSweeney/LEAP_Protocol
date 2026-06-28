# OpENer_uC-NetBurner — ACD, LLDP, and NVDATA scope

Product-scope decisions for **OpENer_uC-NetBurner** (NetBurner-only). Wire module details: **`source/doc/features/LLDP_ACD_WIRE.md`**.

## In-tree implementation

Full **LLDP** (802.1AB + CIP 0x109/0x10A) and **ACD** (RFC 5227 + TCP/IP attrs 10/11) live in **`source/src/ports/netburner/`**:

| Module | Files |
|--------|--------|
| Wire LLDP | `opener_nb_lldp.cpp`, `nndk_overload/nbrtos/source/netrx.cpp` |
| Wire ACD | `opener_nb_acd.cpp`, `opener_nb_nv.cpp` |
| CIP bridge | `opener_nb_lldp_cip.c`, `opener_nb_acd_cip.c` |

Enable with CMake: `-DOPENER_LLDP=ON -DOPENER_NB_LLDP=ON -DOPENER_NB_ACD=ON` (see `.\scripts\build-nb.ps1 -Profile Extended`).

---

## NVDATA — excluded from default firmware bring-up

The `NVDATA` library (`source/src/ports/nvdata/`) uses POSIX `fopen`/`mkdir`. Default **`OPENER_BUILD_NVDATA=OFF`**.

ACD/LLDP persistence uses NNDK **`HalStorage_Read/Save(HalStore_UserParams, …)`** (offsets 512 ACD, LLDP blob) — symbols from **`libnetburner.a`**, declared in `opener_nb_hal_storage.h`.

---

## ACD — implemented when `OPENER_NB_ACD=ON`

- RFC 5227 probe/announce/defend in `opener_nb_acd.cpp`
- ARP hook via `pArpFunc`
- TCP/IP attrs 10/11 via `opener_nb_acd_cip.c`
- `kTcpipCfgCapsAcdCapable` applied when enabled
- Semi-active probes during Class 1 I/O (`OpenerNbAcdNotifyIoConnection` in sample app)
- DHCP rebind: `OpenerNbAcdNotifyDhcpBound()` from `networkconfig.c`

**Boot timing:** `OpenerNbAcdInit()` can block for several seconds during the RFC 5227 probe/announce sequence when ACD is enabled and an address is assigned.

---

## LLDP — implemented when `OPENER_NB_LLDP=ON`

| Layer | Implementation |
|-------|------------------|
| CIP 0x109 / 0x10A | `opener_nb_lldp_cip.c` — dynamic neighbors |
| 0x88CC TX/RX | `opener_nb_lldp.cpp` + poll-driven `nndk_overload/lldp.cpp` |

Firmware must rebuild **`libnetburner.a`** with [`nbrtos/source/netrx.cpp`](../../src/ports/netburner/nndk_overload/nbrtos/source/netrx.cpp) and [`predef-overload.h`](../../src/ports/netburner/nndk_overload/predef-overload.h). Poll-driven `lldp.cpp` is in **`libOPENER_HAL.a`**. See [`porting/README.md`](../porting/README.md).

Skeleton CIP-only path (`ciplldpmanagement.c`) is used only when `OPENER_LLDP=ON` without `OPENER_NB_LLDP` (host/dev, not product).

---

## Multicast TTL

`OpenerHal_SocketSetMulticastTtl()` sets NNDK global `bTTL_Default`. Default CIP `mcast_ttl_value` is **1**. Single-interface modules use `CreateRxTxUdpSocketVia4(..., ifnum)` for outbound interface selection.
