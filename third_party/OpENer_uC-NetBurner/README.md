# OpENer_uC-NetBurner

**OpENer_uC-NetBurner** is a NetBurner-focused EtherNet/IP adapter stack — **embedded only**. Targets NNDK (MODM7AE70, SOMRT1061, and related boards).

**NetBurner team:** start with [`HANDOFF.md`](HANDOFF.md).

**SOMRT1061 NBEclipse bring-up:** see [`INTEGRATION_SOMRT1061.md`](INTEGRATION_SOMRT1061.md) (reference firmware, make hooks, Explorer/EDS).

Status and backlog: [`PROJECT_STATUS.md`](PROJECT_STATUS.md).

## Architecture

```
┌─────────────────────────────────────┐
│  CIP + ENIP core (cip/, enet_encap/)│
├─────────────────────────────────────┤
│  HAL (hal/) — net, timer, memory    │
├─────────────────────────────────────┤
│  NetBurner NNDK (ports/netburner/)  │
└─────────────────────────────────────┘
```

## Build (libraries only)

Build outputs go in `build-*/` at repo root (gitignored).

**Core bring-up:**

```powershell
$env:PATH = "C:\nburn\gcc\bin;$env:PATH"
.\scripts\build-nb.ps1 -Profile Core
```

**Extended (LLDP + ACD in libs):**

```powershell
.\scripts\build-nb.ps1 -Profile Extended
```

**Automated verify:**

```powershell
.\scripts\verify-build.ps1
```

Copy app sources from [`source/src/ports/netburner/app/`](source/src/ports/netburner/app/) and merge [`nbeclipse/makefile.init.snippet`](source/src/ports/netburner/app/nbeclipse/makefile.init.snippet) into your NBEclipse project.

## Configuration

| CMake variable | Default | Purpose |
|----------------|---------|---------|
| `OPENER_NET_BACKEND` | netburner | Use `stub` only with `OPENER_BUILD_NETWORK_LAYER=OFF` (dev compile) |
| `OPENER_NNDK_ROOT` | — | NetBurner SDK root (required) |
| `OPENER_NNDK_PLATFORM` | MODM7AE70 | Board platform folder under `C:/nburn/platform/` |
| `OPENER_LLDP` / `OPENER_NB_LLDP` / `OPENER_NB_ACD` | OFF | Extended tier wire + CIP objects |
| `OPENER_MICRO800_RUN_IDLE_COMPAT` | ON | Rockwell Micro800/Logix run-idle quirks |
| `OPENER_BUILD_PROFILE` | none | `core` or `extended` |
| `OPENER_HAL_MEM_POOL_SIZE` | 32768 | Static pool for `CipCalloc` |

Default stack limits: [`source/src/config/opener_user_conf.h`](source/src/config/opener_user_conf.h) — override via [`app/opener_user_conf.h`](source/src/ports/netburner/app/opener_user_conf.h).

## Documentation

| Doc | Topic |
|-----|-------|
| [HANDOFF.md](HANDOFF.md) | NetBurner team start here |
| [Documentation index](source/doc/README.md) | All guides and feature docs |
| [Application developer guide](source/doc/application/README.md) | Identity, assemblies, callbacks |
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | Backlog |
| [HAL contract](source/doc/hal/README.md) | `OpenerHal_*` |
| [NetBurner integration](source/doc/porting/README.md) | NBEclipse |
| [On-target tests](source/doc/testing/ON_TARGET_TEST_MATRIX.md) | Hardware checklist |
| [LLDP / ACD wire modules](source/doc/features/LLDP_ACD_WIRE.md) | Extended tier LLDP and ACD |
| [Micro800 run-idle compat](source/doc/features/MICRO800_RUN_IDLE_COMPAT.md) | Rockwell I/O header quirks |

