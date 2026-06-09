# LeapOS

Bootable **RTEMS 6.2** image for **Intel D945GSEJT + Atom N270** (32-bit) that runs
the LEAP device stack (`leap-port.exe`) on the `pc386` BSP — full LEAP protocol over
raw L2 on the onboard `re0` NIC with LPT digital I/O. The LEAP app itself lives in the
sibling [LeapPort/](../LeapPort/) tree.

## Quick start

1. Install WSL2 Ubuntu and apt dependencies → [docs/BUILD.md](docs/BUILD.md)
2. One-time RTEMS toolchain: `bash rtems-build/setup-rtems-tree.sh` then `bash rtems-build/rsb-build.sh`
3. **Run-once image (no reboot loop):** `bash rtems-build/build-runonce.sh`
4. Build boot media: `bash rtems-build/build-net.sh` (LEAP device + ISO) or `build-all.sh`
5. Flash **`rtems-image/leapos-rtems-poc.iso`** to CF/USB
6. Boot with **serial COM1 @ 115200 8N1** — default GRUB entry is the LEAP device on serial

From Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File platforms/x86-32/D945GSEJT/LeapOS/rtems-build/build-all.ps1
```

## Documentation

| Doc | Purpose |
| --- | --- |
| [docs/BUILD.md](docs/BUILD.md) | Full build system, paths, scripts, troubleshooting |
| [docs/HARDWARE.md](docs/HARDWARE.md) | D945GSEJT hardware, NIC, boot failure analysis |

## Artifacts (after `build-all.sh`)

```
rtems-image/
  leapos-rtems-poc.img   # Raw MBR+FAT32 — preferred for CF-via-IDE
  leapos-rtems-poc.iso   # Hybrid ISO — USB / Etcher DD
  leap-port.exe          # LEAP device ELF (default boot payload)
  net-probe.exe          # Network bring-up probe ELF
  README.txt             # Flash / boot instructions
```

## Build scripts

```
rtems-build/
  build-all.sh           # Full pipeline (leap-port + ISO + CF .img)
  build-all.ps1          # Windows → WSL wrapper
  build-net.sh           # LEAP device ISO (leap-port + run-once)
  build-leap-port.sh     # leap-port.exe (LEAP device app)
  make-boot-image.sh     # ISO only
  make-cf-image.sh       # CF/IDE raw disk image
  setup-rtems-tree.sh    # Download RTEMS 6.2 sources
  rsb-build.sh           # Toolchain + pc386 BSP (one-time, long)
  check-deps.sh          # Verify host packages
  env.sh                 # Shared path variables
  grub/leapos-grub.cfg   # Serial-first GRUB menu
```

RTEMS toolchain and sources live under `~/rtems/` in WSL (not committed).
