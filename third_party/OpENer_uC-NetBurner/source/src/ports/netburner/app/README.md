# Application layer

Copy these sources into your NBEclipse project (not built by OpENer CMake).

- Integration (makefile, libs): [`source/doc/porting/README.md`](../../../doc/porting/README.md)
- Product I/O map (identity, assemblies, callbacks): [`source/doc/application/README.md`](../../../doc/application/README.md)

| File | Role |
|------|------|
| `opener.c` / `opener.h` | Init, poll, shutdown |
| `networkconfig.c` / `networkconfig.h` | NNDK → CIP network objects |
| `sample_application/sampleapplication.c` | Demo assemblies and callbacks |
| `opener_user_conf.h` | Product limits and tick rate |
| `nbeclipse/main.cpp` | RTOS task |
| `nbeclipse/makefile.init.snippet` | Libs, objects, CPPFLAGS |

Extended tier (LLDP/ACD): [`LLDP_ACD_WIRE.md`](../../../doc/features/LLDP_ACD_WIRE.md). Symlink `nndk_overload/nbrtos` into firmware `overload/` and rebuild `libnetburner.a` — [`porting/README.md`](../../../doc/porting/README.md).
