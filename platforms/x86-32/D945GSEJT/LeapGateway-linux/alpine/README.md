# LeapOS-Gateway — Alpine i386 image

Gateway-only Linux image for D945GSEJT (Atom N270, SATA CF, RTL8111). Target **240 MiB** raw image with headroom on **256 MiB** CF.

## One-time host setup (WSL Ubuntu)

```bash
sudo apt update
sudo apt install -y \
  qemu-user-static binfmt-support \
  wget tar gzip rsync \
  util-linux e2fsprogs grub-pc-bin
sudo update-binfmts --enable qemu-i386 2>/dev/null || true
```

Boot chain is **GRUB i386-pc** (`boot.img` + embedded `core.img`), same as the RTEMS
CF images. Image assembly is **loop-free**: `mke2fs -d` packs the rootfs into
an ext4 partition image in userspace, so it works on WSL2 (whose kernel has no
`loop` module).

## Build image

```bash
cd /mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapGateway-linux/alpine
sed -i 's/\r$//' mk-image.sh
sudo bash mk-image.sh
```

**Caching (default):**
- `cache/alpine-minirootfs-*.tar.gz` — downloaded once (~3 MB)
- `cache/apk/` — Alpine package `.apk` files reused across rebuilds
- `build-work/rootfs/` — full rootfs reused if build succeeded before

Second and later runs **only repack the `.img`** (~30 seconds) unless you change `packages.txt` or set `FORCE_ROOTFS=1`.

```bash
sudo FORCE_ROOTFS=1 bash mk-image.sh   # force full apk/chroot rebuild
```

Output: `LeapOS/rtems-image/leapos-gateway-alpine.img`. By default
`IMAGE_MB=auto` builds the smallest practical image (`rootfs + 32 MiB`, minimum
160 MiB). Override with `IMAGE_MB=240 bash mk-image.sh` when you need a fixed
size.

On first boot, `leap-growfs` grows partition 1 and the ext4 root filesystem to
fill the available CF/USB/HDD/QEMU disk, then stamps
`/var/lib/leap-growfs.done` so it does not run again.

## Gateway daemon (the real port)

`/usr/sbin/leap-gateway` is the **full gateway** ported from the RTEMS build —
LEAP session hub (multi-peer bootstrap + cyclic PD), EIP bridge state, embedded
Web UI on **:8080/:80**, REST API, and config persistence to `/cf/config.txt`.
It shares the RTEMS sources in `../LeapGateway/src/`; the Linux platform layer
(AF_PACKET transport, pthread session thread, POSIX main) lives in `../src/`.

Rebuild after changing gateway sources or `web/index.html`:

```bash
sudo apt install -y gcc-multilib python3   # one-time
cd /mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapGateway-linux
bash build-leap-gateway.sh     # static i386 → alpine/overlay/usr/sbin/leap-gateway
sudo bash alpine/mk-image.sh   # repack image
```

REST endpoints (also used by the Web UI):

```
GET  /                       embedded Web UI
GET  /api/v1/status          runtime status JSON
GET  /api/v1/config          config as key=value text
PUT  /api/v1/config          replace config (validated + applied)
POST /api/v1/config/apply    persist config to /cf/config.txt
POST /api/v1/leap/discover   broadcast LEAP discover scan
POST /api/v1/leap/connect    bootstrap mapped peers
GET  /api/v1/leap/peers      discovered peer table
GET  /api/v1/io              mappings + live I/O
```

EtherNet/IP (OpENer) is **enabled** — the gateway listens on **TCP/UDP 44818**
(encapsulation) and **UDP 2222** (implicit I/O when connected). Assembly mapping
matches the RTEMS build: Input **100** (32 B), Output **150** (32 B), Config **151** (10 B).

OpENer is built from your external `OpENer-Enhanced` checkout (same tree as the RTEMS
gateway). Set `OPENER_ROOT` if it is not at `/mnt/d/OpENer-Enhanced`:

```bash
export OPENER_ROOT=/path/to/OpENer-Enhanced
bash build-opener-linux.sh      # → build-work/libopener_linux.a
bash build-leap-gateway.sh      # links OpENer into leap-gateway
sudo bash alpine/mk-image.sh
```

## LEAP tools on the image

`build-leap-tools.sh` compiles the existing Linux AF_PACKET LEAP tools from
`examples/linux_loopback/` as **static i386 binaries** and drops them into the
overlay. Rebuild the image afterwards to include them:

```bash
sudo apt install -y gcc-multilib cmake   # one-time
bash build-leap-tools.sh
sudo bash mk-image.sh
```

On the gateway (as root):

```sh
leap-discover eth0                  # broadcast HELLO, list LEAP devices
leap-controller eth0                # bootstrap a device session
leap-controller --cyclic eth0       # cyclic PD exchange
leap-controller --diag eth0         # diagnostics read
leap-hub eth0                       # multi-device session hub
leap-selftest                       # on-target leap_core unit/conformance-engine tests
```

The full `leap_conformance` harness is Windows/Npcap-only today; porting it
to Linux is tracked as part of the real gateway daemon work.

## Test in QEMU (before flashing)

```bash
qemu-system-i386 -m 512 \
  -drive file=../../LeapOS/rtems-image/leapos-gateway-alpine.img,format=raw,if=ide \
  -serial stdio
```

Expected: GRUB "loading Alpine kernel...", kernel log on serial, OpenRC,
`leapos-gateway login:` on ttyS0. Log in as `root` (no password).
`rc-status` should show `leap-gateway [started]`.

To reach the Web UI from the host, forward a port to the guest's static IP:

```bash
qemu-system-i386 -m 256 \
  -drive file=../../LeapOS/rtems-image/leapos-gateway-alpine.img,format=raw,if=ide \
  -nic user,model=e1000,net=192.168.1.0/24,host=192.168.1.254,hostfwd=tcp:127.0.0.1:18080-192.168.1.2:8080 \
  -serial stdio

# then on the host:
curl http://127.0.0.1:18080/api/v1/status
```

`serial-probe.py` automates serial-console commands against a QEMU guest
started with `-serial tcp:127.0.0.1:14555,server,nowait` (login + run + capture).

## Flash

Same as RTEMS images — Etcher or `dd` raw to the CF.

## Serial console

COM1 @ **115200 8N1**. You should see OpenRC boot, then `LeapOS-Gateway (Alpine i386)`.

Config: `/cf/config.txt` (default `eth0`, `192.168.1.2`). The daemon assigns the
static IPv4 itself and serves the Web UI at `http://192.168.1.2:8080`.
