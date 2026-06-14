# LeapOS-Gateway — Alpine aarch64 image (Raspberry Pi 4)

Gateway-only Linux image for **Pi 4 / Pi 400 / CM4**. Two-partition layout:
FAT32 boot (VideoCore firmware + `kernel8.img`) and ext4 root (`LABEL=LEAPGW`).

Default image size **512 MiB** — fits small SD cards with headroom for config growth.

## One-time host setup (WSL Ubuntu or native Linux)

```bash
sudo apt update
sudo apt install -y \
  qemu-user-static binfmt-support \
  wget tar gzip rsync \
  util-linux e2fsprogs mtools \
  gcc-aarch64-linux-gnu cmake make python3
sudo update-binfmts --enable qemu-aarch64 2>/dev/null || true
```

Image assembly is **loop-free** (`mke2fs -d` + `mtools`) so it works on WSL2.

On **native aarch64** (e.g. Pi 4 or Apple Silicon Linux VM), the chroot runs
natively — `qemu-aarch64-static` is not required.

## Build real gateway + image

```bash
cd platforms/raspberry-pi/Pi4/LeapGateway-linux
sed -i 's/\r$//' build-opener-linux.sh build-leap-gateway.sh build-leap-tools.sh alpine/mk-image.sh
bash build-leap-gateway.sh     # static aarch64 leap-gateway -> alpine/overlay/usr/sbin
bash build-leap-tools.sh       # static aarch64 LEAP tools -> alpine/overlay/usr/sbin
sudo bash alpine/mk-image.sh
```

**Caching (default):**

- `alpine/cache/alpine-minirootfs-*.tar.gz` — downloaded once
- `alpine/cache/apk/` — Alpine package `.apk` files reused across rebuilds
- `alpine/build-work/rootfs/` — full rootfs reused if build succeeded before

Second and later runs **only repack the `.img`** unless you change `packages.txt`
or set `FORCE_ROOTFS=1`.

```bash
sudo FORCE_ROOTFS=1 bash alpine/mk-image.sh   # force full apk/chroot rebuild
```

Output: `../image/leapos-gateway-alpine.img` from `LeapGateway-linux/`.

On first boot, `leap-growfs` grows the ext4 root partition (`LABEL=LEAPGW`) to
fill the SD card, then stamps `/var/lib/leap-growfs.done` so it only runs once.

The Raspberry Pi firmware splash is disabled (`disable_splash=1`). During OpenRC
boot, `leap-splash` paints a LeapOS-Gateway banner on `tty1`.

## Gateway daemon status

`/usr/sbin/leap-gateway` is now built from the shared x86 gateway application
sources plus the Linux AF_PACKET/POSIX platform layer. This brings up:

- LEAP session hub: discovery, connect, multi-peer bootstrap, cyclic PD
- Embedded Web UI and REST API on `:8080` / `:80`
- Config load/save through `/cf/config.txt`
- Static IPv4 assignment from gateway config

EtherNet/IP parity uses the external OpENer checkout at `/mnt/d/OpENer-Enhanced`.
Build the gateway with OpENer enabled:

```bash
OPENER_ROOT=/mnt/d/OpENer-Enhanced LEAP_GATEWAY_OPENER_ENABLE=1 bash build-leap-gateway.sh
sudo bash alpine/mk-image.sh
```

`build-opener-linux.sh` stages the OpENer source under `build-work/`, overlays
the gateway-specific Linux platform port, builds static aarch64 OpENer archives,
and combines them into `build-work/libopener_linux_aarch64.a`.

## LEAP tools on the image

On the gateway (as root):

```sh
leap-discover eth0
leap-controller eth0
leap-controller --cyclic eth0
leap-hub eth0
leap-selftest
```

## Flash

Write the raw image to microSD:

```bash
# Linux
sudo dd if=../image/leapos-gateway-alpine.img of=/dev/sdX bs=4M status=progress conv=fsync
```

Or use Raspberry Pi Imager → **Use custom** → select the `.img`.

## Serial console

GPIO UART (`serial0`) @ **115200 8N1** with `enable_uart=1` in firmware config.
Connect USB-serial to GPIO 14 (TXD) / 15 (RXD) / GND.

Expected boot: OpenRC messages, `leapos-gateway login:`, log in as `root` (no password).

Config: `/cf/config.txt` (default `eth0`, `192.168.1.3`).

Initial parity checks:

```sh
rc-status
ps | grep leap-gateway
wget -qO- http://192.168.1.3:8080/api/v1/status
leap-discover eth0
leap-controller --cyclic eth0
```
