# NNDK overload for Extended tier (LLDP/ACD)

Mirror NetBurner install paths under your NBEclipse **`overload/`** folder. Symlink or copy **`nbrtos/`** from this directory into your firmware project:

```
your-firmware/overload/nbrtos  →  OpENer_uC-NetBurner/source/src/ports/netburner/nndk_overload/nbrtos
```

Or copy these files manually:

| In-tree path | Firmware overload path |
|--------------|------------------------|
| `nbrtos/include/predef-overload.h` | `overload/nbrtos/include/predef-overload.h` |
| `nbrtos/source/netrx.cpp` | `overload/nbrtos/source/netrx.cpp` |

Root [`predef-overload.h`](predef-overload.h) duplicates the include file for makefile/CMake references.

## Required defines

- **`ALLOW_CUSTOM_NET_DO_RX (1)`** — enables `CustomNetDoRX` / `SetCustomNetDoRX` for ethertype **0x88CC**
- **`ENABLE_SNMP (1)`** — NNDK `LLDPEntity` TX path

After changing predef or `netrx.cpp`, run **Clean NetBurner Archive** in NBEclipse, then rebuild.

## netrx.cpp

[`nbrtos/source/netrx.cpp`](nbrtos/source/netrx.cpp) replaces stock `NetDoRX` in **`libnetburner.a`** only (MKNBLIBS). It calls `CustomNetDoRX` **before** the `< 0x600` discard path so LLDP frames reach `opener_nb_lldp.cpp`.

**Do not** add `netrx.o` to `USER_OBJS` — that duplicates `NetDoRX`.

## Poll-driven LLDP TX

`lldp.cpp` in this folder is compiled into **`libOPENER_HAL.a`** by `.\scripts\build-nb.ps1 -Profile Extended`. Do not link a separate `lldp.o` in the firmware makefile.

See [`source/doc/porting/README.md`](../../../doc/porting/README.md).
