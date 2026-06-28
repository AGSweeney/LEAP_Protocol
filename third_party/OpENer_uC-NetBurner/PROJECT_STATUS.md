# OpENer_uC-NetBurner — project status

Libraries and NetBurner port are in-tree. **NetBurner team handoff:** [`HANDOFF.md`](HANDOFF.md).

Build commands and CMake options: [`README.md`](README.md). Integration steps: [`source/doc/porting/README.md`](source/doc/porting/README.md).

---

## Backlog

### Before first flash

- [ ] NBEclipse project linking `build-nb/lib/` and `source/src/ports/netburner/app/`
- [ ] Product EDS/STC (do not ship `data/OpENerPC.stc` unchanged)
- [ ] Identity vendor ID, product code, serial number policy

### After Core tier works on hardware

- [ ] Symlink/copy `nndk_overload/nbrtos` → firmware `overload/nbrtos`; rebuild **`libnetburner.a`** (predef + netrx.cpp)
- [ ] Validate LLDP (0x88CC) and ACD on wire
- [ ] STC/EDS updated for classes 0x109/0x10A when using Extended tier

### Later

- [ ] QoS DSCP on NNDK sockets (`OpenerHal_SocketSetQoS`)
- [ ] Zero-copy UDP receive path
- [ ] ACD `needs_reset` policy after Set attribute 10

---

## Open items

| Item | Notes |
|------|--------|
| On-target validation | Not run — [`ON_TARGET_TEST_MATRIX.md`](source/doc/testing/ON_TARGET_TEST_MATRIX.md) |
| QoS marking | SetAttribute OK; traffic not marked |
| Demo STC | `data/OpENerPC.stc` is reference only |

---

## ACD boot

With `OPENER_NB_ACD=ON`, `OpenerNbAcdInit()` may block several seconds during RFC 5227 probe before the CIP stack starts.

NV: `HalStorage_Read/Save(HalStore_UserParams, …)` — ACD at offset 512, LLDP blob follows (`opener_nb_nv.cpp`).
