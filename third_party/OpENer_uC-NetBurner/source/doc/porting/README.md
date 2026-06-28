# OpENer_uC-NetBurner integration guide

Integrate static libraries from `build-nb-<PLATFORM>/lib/` into an NBEclipse project. **Overview:** [`HANDOFF.md`](../../../HANDOFF.md) at repo root.

## Steps

1. **Build libs** — For OpENer4NetBurner example projects created from NBEclipse, libraries build automatically on first link (no manual command). Standalone integrations can build the same libraries with the CMake toolchain flow in [`README.md`](../../../README.md).

2. **Copy app** — [`source/src/ports/netburner/app/`](../../src/ports/netburner/app/): `opener.c`, `networkconfig.c`, `sampleapplication.c`, `opener_user_conf.h`, `nbeclipse/main.cpp`.

3. **Makefile** — merge [`app/nbeclipse/makefile.init.snippet`](../../src/ports/netburner/app/nbeclipse/makefile.init.snippet); set `OPENER_ROOT`.

4. **Include order** — `opener_user_conf.h` before [`source/src/config/opener_user_conf.h`](../../src/config/opener_user_conf.h).

5. **Link** — `CIP`, `ENET_ENCAP`, `Utils`, `PLATFORM_GENERIC`, `OPENER_HAL`. Skip `NVDATA` for bring-up.

6. **Run** — RTOS task calls `opener_init(ifnum)` once, then `opener_process()` every `kOpenerTimerTickInMilliSeconds` (default 50 ms).

Extended tier: see [`LLDP_ACD_WIRE.md`](../features/LLDP_ACD_WIRE.md).

## Extended tier — rebuild `libnetburner.a`

Wire LLDP RX needs **`ALLOW_CUSTOM_NET_DO_RX (1)`**, **`ENABLE_SNMP (1)`**, and the in-tree **`netrx.cpp`** overload. Files live under [`nndk_overload/nbrtos/`](../../src/ports/netburner/nndk_overload/nbrtos/) (mirror NNDK `overload/nbrtos/...` layout).

1. Symlink or copy **`nndk_overload/nbrtos`** into your firmware project as **`overload/nbrtos`** (see [`nndk_overload/README.md`](../../src/ports/netburner/nndk_overload/README.md)).

2. Point **USER_PREDEF** at `predef-overload.h` (in `overload/nbrtos/include/` or [`nndk_overload/predef-overload.h`](../../src/ports/netburner/nndk_overload/predef-overload.h)).

3. Merge the archive stale rule from `makefile.init.snippet` (depends on predef + `netrx.cpp`):
   ```makefile
   $(OBJDIR)/libnetburner.a: $(OPENER_PREDEF) $(OPENER_NETRX)
   	@touch $(OBJDIR)/.stale
   ```

4. **Clean NetBurner Archive** in NBEclipse, then rebuild the firmware project.

5. **Do not** add `netrx.o` or `lldp.o` to `USER_OBJS` — `netrx.cpp` goes into **`libnetburner.a`** via MKNBLIBS; poll-driven `lldp.cpp` is in **`libOPENER_HAL.a`**.

`netrx.cpp` calls `CustomNetDoRX` before ethertype dispatch so **0x88CC** LLDP frames are not discarded as unknown protocols.

## CMake (manual build)

```cmake
cmake -S source -B build-nb \
  -DCMAKE_TOOLCHAIN_FILE=source/buildsupport/Toolchain/Toolchain-NetBurner-NNDK.cmake \
  -DOPENER_NET_BACKEND=netburner \
  -DOPENER_NNDK_ROOT=C:/nburn \
  -DOPENER_BUILD_NETWORK_LAYER=ON
```

Use `C:\nburn\gcc\` for cross-compile.

## Boards

Set `OPENER_NNDK_PLATFORM` (e.g. `MODM7AE70`, `SOMRT1061`). Interface numbers are 1-based NNDK indices passed to `opener_init()`.
