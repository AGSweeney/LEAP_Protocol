# LeapPort (D945GSEJT / RTEMS)

RTEMS waf application for the LEAP device stack on Intel D945GSEJT (pc386 BSP).

## Status

Working — full LEAP device stack (DISC/DIR/MGMT/PD/DIAG) over raw L2 on the onboard
`re0` NIC, with LPT-port digital I/O (8 outputs / 5 inputs, presented as the 8×8
profile). Usually built and run via the
**LeapOS** boot image (`../LeapOS/rtems-build/build-all.sh`); the `./waf` flow below
builds the same app standalone.

## Quick build

```bash
./scripts/gen_build_info.sh   # or gen_build_info.ps1 on Windows
./waf configure --rtems-bsp=pc386/pc386 --rtems-version=6
./waf
```

Set `RTEMS_TOOLS` and `RTEMS_BSPS` to your RTEMS 6 install prefix before
configure if they are not on `PATH`.

## Source map

| File | Role |
| --- | --- |
| `src/init.c` | RTEMS `Init` task, stack setup, main loop |
| `src/leap_transport.c` | Raw L2 over libbsd BPF on `re0` |
| `src/leap_board.c` | LPT-port digital I/O (8 out / 5 in) |
| `src/leap_time.c` | `clock_gettime(CLOCK_MONOTONIC)` wrapper |

Linked from `leap_core/`: device stack, services, frame, CRC, log, build info.
