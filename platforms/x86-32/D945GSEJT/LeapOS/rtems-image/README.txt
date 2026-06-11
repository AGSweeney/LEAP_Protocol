LeapOS-Gateway (Alpine i386) — D945GSEJT
=========================================

Target: Intel D945GSEJT + Atom N270, CF on SATA0, 256 MB card.

Artifact
--------
  leapos-gateway-alpine.img   Gateway-only Linux image (~128 MB; flash to CF)

Flash to CF
-----------
  balenaEtcher: select leapos-gateway-alpine.img (raw block write)

  Linux/WSL:
  sudo dd if=leapos-gateway-alpine.img of=/dev/sdX bs=4M status=progress conv=fsync

Boot
----
1. BIOS: SATA/CF first, legacy boot, LBA enabled.
2. Serial COM1 @ 115200 8N1 — OpenRC boot log and leap-gateway stub.
3. Default IP from /cf/config.txt: 192.168.1.2 (eth0).

Rebuild
-------
  cd platforms/x86-32/D945GSEJT/LeapGateway-linux/alpine
  sudo bash mk-image.sh

See LeapGateway-linux/alpine/README.md
