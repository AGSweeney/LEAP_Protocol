#!/bin/bash
# Write rtems-image/README.txt with repo-relative paths.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

cat >"$LEAPOS_IMAGE_DIR/README.txt" <<EOF
LeapOS (pc386 / i386) — Device and Gateway images
=================================================

Targets: Intel D945GSEJT + Atom N270 (CF-via-IDE or USB boot).

Each boot image contains **one product only** (Device or Gateway).

Artifacts
---------
  leapos-device.iso      LeapOS-Device ISO (leap-port.exe)
  leapos-device.img      LeapOS-Device CF/IDE disk image
  leapos-gateway.iso     LeapOS-Gateway ISO (leap-eip-gateway.exe)
  leapos-gateway.img     LeapOS-Gateway CF/IDE disk image
  leap-port.exe          LeapOS-Device ELF
  leap-eip-gateway.exe   LeapOS-Gateway ELF (E/IP bridge)
  net-probe.exe          Network probe ELF (not on boot images; build separately)

Flash to CF / USB
-----------------
D945GSEJT primary boot path is **CF on parallel IDE**. Prefer the .img for CF:

  sudo dd if=leapos-device.img of=/dev/sdX bs=4M status=progress conv=fsync
  sudo dd if=leapos-gateway.img of=/dev/sdX bs=4M status=progress conv=fsync

For USB sticks:

  sudo dd if=leapos-device.iso of=/dev/sdX bs=4M status=progress conv=fsync
  sudo dd if=leapos-gateway.iso of=/dev/sdX bs=4M status=progress conv=fsync

Boot the D945GSEJT
------------------
1. BIOS: IDE/CF first, legacy boot, LBA enabled; disable TCO watchdog if present.
2. Connect serial COM1 @ 115200 8N1 — default GRUB entry uses serial console.
3. **Device image**: boots leap-port.exe (LEAP device + LPT I/O).
4. **Gateway image**: boots leap-eip-gateway.exe (HTTP :8080, LEAP controller).

VGA notes (945GSE IGP)
----------------------
RTEMS VBE is disabled (945GSE: "VBE Core not implemented").
VGA text mode uses /dev/vgacons with --video=off.

Rebuild
-------
  cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
  bash build-all.sh iso-device     # device ISO only
  bash build-all.sh iso-gateway    # gateway ISO only
  bash build-all.sh iso            # both ISOs
  bash build-all.sh all            # ELFs + both ISOs + both CF images

See LeapGateway/README.md and LeapOS docs in the platform tree.
EOF
