# LeapOS

Bootable **RTEMS 6.2** image for **Intel D945GSEJT + Atom N270** (32-bit) that runs
the LEAP device stack (`leap-port.exe`) on the `pc386` BSP — full LEAP protocol over
raw L2 on the onboard `re0` NIC with LPT digital I/O. The LEAP app itself lives in the
sibling [LeapPort/](../LeapPort/) tree.

## Quick start

1. Install WSL2 Ubuntu and apt dependencies → [docs/BUILD.md](docs/BUILD.md)
2. One-time RTEMS toolchain: `bash rtems-build/setup-rtems-tree.sh` then `bash rtems-build/rsb-build.sh`
3. **Run-once image (no reboot loop):** `bash rtems-build/build-runonce.sh`
4. Build boot media: `bash rtems-build/build-all.sh iso-device` (Device) or `iso-gateway` (Gateway)
5. Flash **`rtems-image/leapos-device.iso`** or **`leapos-gateway.iso`** to CF/USB
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
  leapos-device.img    # LeapOS-Device CF/IDE image
  leapos-device.iso    # LeapOS-Device USB ISO
  leapos-gateway.img   # LeapOS-Gateway CF/IDE image
  leapos-gateway.iso   # LeapOS-Gateway USB ISO
  leap-port.exe        # Device ELF
  leap-eip-gateway.exe # Gateway ELF
  net-probe.exe        # Network probe ELF (not on boot images)
  README.txt           # Flash / boot instructions
```

## Build scripts

```
rtems-build/
  build-all.sh           # Full pipeline (both products + ISOs + CF images)
  build-all.ps1          # Windows → WSL wrapper
  build-net.sh           # LEAP device ISO (leap-port + run-once)
  build-leap-port.sh     # leap-port.exe (LeapOS-Device)
  build-leap-eip-gateway.sh # leap-eip-gateway.exe (LeapOS-Gateway)
  make-device-iso.sh     # leapos-device.iso
  make-gateway-iso.sh    # leapos-gateway.iso
  make-cf-image.sh       # leapos-device.img or leapos-gateway.img
  setup-rtems-tree.sh    # Download RTEMS 6.2 sources
  rsb-build.sh           # Toolchain + pc386 BSP (one-time, long)
  check-deps.sh          # Verify host packages
  env.sh                 # Shared path variables
  grub/leapos-device-grub.cfg
  grub/leapos-gateway-grub.cfg
```

RTEMS toolchain and sources live under `~/rtems/` in WSL (not committed).
