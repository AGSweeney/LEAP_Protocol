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
  util-linux genext2fs mtools
sudo update-binfmts --enable qemu-aarch64 2>/dev/null || true
```

Image assembly is **loop-free** (`genext2fs` + `mtools`) so it works on WSL2.

On **native aarch64** (e.g. Pi 4 or Apple Silicon Linux VM), the chroot runs
natively — `qemu-aarch64-static` is not required.

## Build image

```bash
cd platforms/raspberry-pi/Pi4/LeapGateway-linux/alpine
sed -i 's/\r$//' mk-image.sh
sudo bash mk-image.sh
```

**Caching (default):**

- `cache/alpine-minirootfs-*.tar.gz` — downloaded once
- `cache/apk/` — Alpine package `.apk` files reused across rebuilds
- `build-work/rootfs/` — full rootfs reused if build succeeded before

Second and later runs **only repack the `.img`** unless you change `packages.txt`
or set `FORCE_ROOTFS=1`.

```bash
sudo FORCE_ROOTFS=1 bash mk-image.sh   # force full apk/chroot rebuild
```

Output: `../../image/leapos-gateway-alpine.img`

## LEAP tools on the image

```bash
sudo apt install -y gcc-aarch64-linux-gnu cmake   # cross-build from x86_64 host
bash build-leap-tools.sh
sudo bash mk-image.sh
```

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
sudo dd if=../../image/leapos-gateway-alpine.img of=/dev/sdX bs=4M status=progress conv=fsync
```

Or use Raspberry Pi Imager → **Use custom** → select the `.img`.

## Serial console

GPIO UART (`serial0`) @ **115200 8N1** with `enable_uart=1` in firmware config.
Connect USB-serial to GPIO 14 (TXD) / 15 (RXD) / GND.

Expected boot: OpenRC messages, `leapos-gateway login:`, log in as `root` (no password).

Config: `/cf/config.txt` (default `eth0`, `192.168.1.2`).

The current `leap-gateway` binary is a **stub** (network up + heartbeat). Full
LEAP/EIP/Web port follows the x86 gateway sources.
