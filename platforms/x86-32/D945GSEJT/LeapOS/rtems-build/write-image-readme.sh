#!/bin/bash
# Write rtems-image/README.txt with repo-relative paths.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

cat >"$LEAPOS_IMAGE_DIR/README.txt" <<EOF
LeapOS (pc386 / i386) — LeapPort device images
==============================================

Targets: Intel D945GSEJT + Atom N270 (CF-via-IDE or USB boot).

The LEAP gateway product is Alpine Linux — see LeapGateway-linux/ and
leapos-gateway-alpine.img (built separately from this RTEMS pipeline).

Artifacts
---------
  leapos-device.iso      LeapOS-Device ISO (leap-port.exe)
  leapos-device.img      LeapOS-Device CF/IDE disk image
  leap-port.exe          LeapOS-Device ELF
  net-probe.exe          Network probe ELF (not on boot images; build separately)

Flash to CF / USB
-----------------
D945GSEJT primary boot path is **CF on parallel IDE**. Prefer the .img for CF:

  sudo dd if=leapos-device.img of=/dev/sdX bs=4M status=progress conv=fsync

For USB sticks:

  sudo dd if=leapos-device.iso of=/dev/sdX bs=4M status=progress conv=fsync

Boot the D945GSEJT
------------------
1. BIOS: IDE/CF first, legacy boot, LBA enabled; disable TCO watchdog if present.
2. Connect serial COM1 @ 115200 8N1 — default GRUB entry uses serial console.
3. Device image boots leap-port.exe (LEAP device + LPT I/O).

VGA notes (945GSE IGP)
----------------------
RTEMS VBE is disabled (945GSE: "VBE Core not implemented").
VGA text mode uses /dev/vgacons with --video=off.

Rebuild
-------
  cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
  bash build-all.sh iso-device     # device ISO only
  bash build-all.sh cf-device      # device CF image only
  bash build-all.sh all            # ELF + ISO + CF image

Gateway (Alpine Linux):
  cd platforms/x86-32/D945GSEJT/LeapGateway-linux
  bash build-leap-gateway.sh
  sudo bash alpine/mk-image.sh

Alpine device (Linux alternative to RTEMS leap-port):
  cd platforms/x86-32/D945GSEJT/LeapDevice-linux
  bash build-leap-device.sh
  sudo bash alpine/mk-image.sh

See LeapPort/, LeapDevice-linux/, LeapGateway-linux/, and LeapOS docs.
EOF
