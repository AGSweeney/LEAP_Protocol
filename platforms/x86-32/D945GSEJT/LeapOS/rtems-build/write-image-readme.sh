#!/bin/bash
# Write rtems-image/README.txt with repo-relative paths.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

cat >"$LEAPOS_IMAGE_DIR/README.txt" <<EOF
LeapOS Proof-of-Concept (pc386 / i386)
======================================

Targets: Intel D945GSEJT + Atom N270 (CF-via-IDE or USB boot).

Artifacts
---------
  leapos-rtems-poc.iso   Hybrid ISO (GRUB + net-probe.exe) — USB or Etcher DD
  leapos-rtems-poc.img   Raw MBR+FAT32 disk image — preferred for CF/IDE
  net-probe.exe          LeapOS network probe ELF

Flash to CF / USB
-----------------
D945GSEJT primary boot path is **CF on parallel IDE**. Prefer the .img when
using a CF-to-IDE adapter:

  sudo dd if=leapos-rtems-poc.img of=/dev/sdX bs=4M status=progress conv=fsync

For USB sticks or when .img is unavailable:

  - Rufus: select leapos-rtems-poc.iso, MBR, DD/ISO mode
  - balenaEtcher: Flash from file
  - Linux: sudo dd if=leapos-rtems-poc.iso of=/dev/sdX bs=4M status=progress conv=fsync

Boot the D945GSEJT
------------------
1. BIOS: IDE/CF first, legacy boot, LBA enabled; disable TCO watchdog if present.
2. Connect serial COM1 @ 115200 8N1 — default GRUB entry uses serial console.
3. Optional: VGA monitor for the VGA text menu entry.
4. Default boot: **LeapOS net-probe (serial COM1 @ 115200)**.
5. Expected net-probe output:

     *** LeapOS net-probe ***
     Network stack bring-up (libbsd)
     ...
     *** LeapOS net-probe complete — halting ***

VGA notes (945GSE IGP)
----------------------
This image disables RTEMS VBE (Intel 945GSE returns "VBE Core not implemented").
VGA text mode uses /dev/vgacons with --video=off. Do not use bare multiboot
without --video=off on this board.

Manual GRUB (press c at menu)
------------------------------
  ls
  ls (hd0,msdos1)/
  multiboot (hd0,msdos1)/net-probe.exe --video=off --console=/dev/com1,115200 --printk=/dev/com1,115200
  boot

QEMU smoke test (host)
----------------------
  cd rtems-image
  qemu-system-i386 -m 128 -no-reboot -nographic \\
    -append "--video=off --console=/dev/com1,115200" -kernel net-probe.exe

Rebuild
-------
  cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
  bash build-net.sh

See docs/BUILD.md and docs/HARDWARE.md in the LeapOS tree.
EOF
