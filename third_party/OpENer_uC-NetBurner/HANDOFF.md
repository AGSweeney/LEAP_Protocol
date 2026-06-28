# OpENer_uC-NetBurner — NetBurner team handoff

EtherNet/IP adapter stack for NetBurner NNDK: OpENer CIP/ENIP core, `OpenerHal_*` abstraction, and a NetBurner-specific port (HAL, app layer, optional LLDP/ACD).

This package ships **source + build scripts**. Static libraries are **not** included — generate them on a Windows host with NNDK installed.

---

## Prerequisites

| Requirement | Notes |
|-------------|--------|
| **NNDK** | Default path `C:/nburn`; must contain `nbrtos/include` |
| **Toolchain** | `C:/nburn/gcc/bin` on `PATH` for cross-compile |
| **Host** | Windows + PowerShell (build scripts) |
| **Target boards** | Tested against `MODM7AE70`; `SOMRT1061` — see [`INTEGRATION_SOMRT1061.md`](INTEGRATION_SOMRT1061.md) for full NBEclipse wiring |

Set platform when building:

```powershell
.\scripts\build-nb.ps1 -Profile Core -Platform SOMRT1061
```

---

## Quick start

### 1. Build OpENer libraries

From the repo root:

```powershell
$env:PATH = "C:\nburn\gcc\bin;$env:PATH"
.\scripts\verify-build.ps1          # optional: stub + NetBurner configure/build smoke test
.\scripts\build-nb.ps1 -Profile Core
```

Output: `build-nb/lib/*.a`

**Extended tier** (LLDP + ACD CIP objects and wire modules):

```powershell
.\scripts\build-nb.ps1 -Profile Extended
```

Also merge Extended `CPPFLAGS` from [`makefile.init.snippet`](source/src/ports/netburner/app/nbeclipse/makefile.init.snippet) and rebuild **`libnetburner.a`** (see below).

### 2. Create an NBEclipse firmware project

There is **no** in-repo NBEclipse project. Integrate into your firmware as follows:

1. Copy or reference app sources under [`source/src/ports/netburner/app/`](source/src/ports/netburner/app/).
2. Merge [`source/src/ports/netburner/app/nbeclipse/makefile.init.snippet`](source/src/ports/netburner/app/nbeclipse/makefile.init.snippet) into `makefile.init`.
3. Set **`OPENER_ROOT`** to this checkout path.
4. Link libraries from **`$(OPENER_ROOT)/build-nb/lib/`**:

   | Library | Role |
   |---------|------|
   | `libCIP.a` | CIP stack |
   | `libENET_ENCAP.a` | EtherNet/IP encapsulation |
   | `libUtils.a` | Utilities |
   | `libOPENER_HAL.a` | NetBurner HAL + Extended wire code |
   | `libPLATFORM_GENERIC.a` | Network handler |

   Do **not** link `libNVDATA.a` for bring-up (POSIX NV; not used on NNDK).

5. Ensure **`opener_user_conf.h`** (app copy) is on the include path **before** [`source/src/config/opener_user_conf.h`](source/src/config/opener_user_conf.h).

6. Run **`opener_init(ifnum)`** once from an RTOS task, then **`opener_process()`** every **50 ms** — see [`nbeclipse/main.cpp`](source/src/ports/netburner/app/nbeclipse/main.cpp).

Full steps: [`source/doc/porting/README.md`](source/doc/porting/README.md).

### 3. Flash and validate

Use [`source/doc/testing/ON_TARGET_TEST_MATRIX.md`](source/doc/testing/ON_TARGET_TEST_MATRIX.md):

- Test 1 — List Identity (UDP 44818)
- Test 2 — Explicit messaging (TCP 44818)
- Test 3 — I/O connection (UDP 2222; demo loops assembly 150 → 100)

---

## Repository layout

```
OpENer_uC-NetBurner/
├── HANDOFF.md                 ← this document
├── README.md                  ← build options summary
├── PROJECT_STATUS.md          ← backlog / open items
├── scripts/
│   ├── build-nb.ps1           ← cross-compile libs (Core / Extended)
│   └── verify-build.ps1       ← host + NetBurner smoke test
├── data/
│   ├── OpENerPC.stc           ← reference STC only — do not ship unchanged
│   └── opener_sample_app.eds  ← reference EDS fragment
└── source/
    ├── CMakeLists.txt
    ├── doc/                   ← integration, HAL, application, testing guides
    └── src/
        ├── cip/               ← CIP core
        ├── enet_encap/        ← ENIP
        ├── hal/               ← OpenerHal contract
        └── ports/netburner/   ← NetBurner port, app, nndk_overload
```

Build artifacts (`build-nb/`, etc.) are gitignored and not shipped.

---

## Build profiles

| Profile | CMake | Libraries / features |
|---------|--------|----------------------|
| **Core** | `-DOPENER_BUILD_PROFILE=core` | Identity, TCP/IP, Ethernet Link, Assembly, Connection Manager, Micro800 run-idle compat |
| **Extended** | `-DOPENER_BUILD_PROFILE=extended` + LLDP/ACD flags | Core + CIP **0x109/0x10A** (LLDP), TCP/IP attrs **10/11** (ACD), wire modules in `libOPENER_HAL.a` |

Identity defaults (override at CMake configure time):

| Variable | Default |
|----------|---------|
| `OpENer_Device_Config_Vendor_Id` | `1` |
| `OpENer_Device_Config_Product_Code` | `65001` |
| `OpENer_Device_Config_Device_Name` | `"OpENer Device"` |
| `OpENer_Device_Config_Device_Type` | `12` (Communications Adapter) |

Serial number at runtime is derived from MAC bytes 2–5 in `networkconfig.c`.

---

## Extended tier — `libnetburner.a` overload

LLDP receive (ethertype **0x88CC**) requires NNDK **`netrx.cpp`** overload and predef flags in **`libnetburner.a`**, not in OpENer static libs alone.

1. Symlink or copy [`source/src/ports/netburner/nndk_overload/nbrtos`](source/src/ports/netburner/nndk_overload/nbrtos) → **`your-firmware/overload/nbrtos`**
2. Set **`USER_PREDEF`** to `predef-overload.h` (`ALLOW_CUSTOM_NET_DO_RX`, `ENABLE_SNMP`)
3. Add archive stale rule from `makefile.init.snippet`
4. **Clean NetBurner Archive** in NBEclipse, rebuild firmware
5. **Do not** add `netrx.o` or `lldp.o` to `USER_OBJS`

Details: [`source/doc/porting/README.md`](source/doc/porting/README.md), [`source/src/ports/netburner/nndk_overload/README.md`](source/src/ports/netburner/nndk_overload/README.md).

**ACD boot:** with Extended enabled, `OpenerNbAcdInit()` may block several seconds during RFC 5227 probe before the CIP stack accepts connections.

---

## Customizing the product

Replace the demo application — assemblies, connection points, and CIP callbacks:

| Topic | Guide |
|-------|--------|
| Identity, assemblies, callbacks | [`source/doc/application/README.md`](source/doc/application/README.md) |
| Stack limits (sessions, tick rate) | [`source/src/ports/netburner/app/opener_user_conf.h`](source/src/ports/netburner/app/opener_user_conf.h) |
| EDS / STC | Regenerate for your vendor ID, product code, assembly instances — **do not** ship `data/OpENerPC.stc` unchanged |

Demo assembly map (reference):

| Instance | Direction | Size | Role |
|----------|-----------|------|------|
| 100 | T→O | 32 | Input to scanner |
| 150 | O→T | 32 | Output from scanner |
| 151 | Config | 10 | Configuration |
| 152 / 153 | — | 0 | Heartbeat placeholders |
| 154 | Explicit | 32 | Optional explicit assembly |

---

## Validation status

| Area | Status |
|------|--------|
| Host stub build | Covered by `verify-build.ps1` |
| NetBurner cross-compile | Covered by `verify-build.ps1` when `C:/nburn` present |
| On-target List Identity / explicit / I/O | **Not run by upstream** — NetBurner team to execute test matrix |
| Extended LLDP/ACD on wire | **Not run by upstream** — after Core tier passes |

Record results in the test matrix table at the bottom of [`ON_TARGET_TEST_MATRIX.md`](source/doc/testing/ON_TARGET_TEST_MATRIX.md).

---

## Known limitations

| Item | Notes |
|------|--------|
| **QoS DSCP** | CIP QoS object accepts SetAttribute; `OpenerHal_SocketSetQoS` is currently a no-op |
| **File Object (0x37)** | **Disabled** by default (`CIP_FILE_OBJECT 0`; CMake `OpENer_CIP_OBJECT_CIP_FILE_OBJECT=OFF`) |
| **NVDATA library** | Excluded; ACD/LLDP persistence uses NNDK `HalStorage` (`opener_nb_nv.cpp`) |
| **Zero-copy UDP** | Not implemented; standard recv path only |
| **Reference STC/EDS** | Demo identity and assemblies only |

Future work: [`PROJECT_STATUS.md`](PROJECT_STATUS.md).

---

## Documentation index

| Document | Purpose |
|----------|---------|
| [`source/doc/README.md`](source/doc/README.md) | Full doc index |
| [`source/doc/porting/README.md`](source/doc/porting/README.md) | NBEclipse integration |
| [`source/doc/application/README.md`](source/doc/application/README.md) | Product application layer |
| [`source/doc/hal/README.md`](source/doc/hal/README.md) | `OpenerHal_*` contract |
| [`source/doc/testing/ON_TARGET_TEST_MATRIX.md`](source/doc/testing/ON_TARGET_TEST_MATRIX.md) | Hardware checklist |
| [`source/doc/features/LLDP_ACD_WIRE.md`](source/doc/features/LLDP_ACD_WIRE.md) | Extended tier wire modules |
| [`source/doc/features/SCOPE_ACD_NVDATA.md`](source/doc/features/SCOPE_ACD_NVDATA.md) | ACD/LLDP scope and NV |
| [`source/doc/features/MICRO800_RUN_IDLE_COMPAT.md`](source/doc/features/MICRO800_RUN_IDLE_COMPAT.md) | Rockwell I/O header quirks |
| [`source/src/ports/netburner/README.md`](source/src/ports/netburner/README.md) | HAL backend mapping |

---

## Suggested NetBurner team checklist

- [ ] Install NNDK; run `.\scripts\verify-build.ps1` and `.\scripts\build-nb.ps1 -Profile Core`
- [ ] Wire NBEclipse project per porting guide; set `OPENER_ROOT`
- [ ] First flash; complete test matrix Tests 1–3
- [ ] Set product identity (CMake + EDS/STC)
- [ ] Replace `sampleapplication.c` with product I/O map
- [ ] (Optional) Extended tier: overload `libnetburner.a`, validate LLDP/ACD, update STC for 0x109/0x10A

---

## License

See [`license.txt`](license.txt). OpENer core and NetBurner port follow adapted BSD-style terms; third-party objects (e.g. File Object) have separate license files under their directories.
