# LeapOS NetBoot Server

Alpine Linux **i386** appliance for the Intel **D945GSEJT** lab boards. It serves
PXE/TFTP, HTTP boot files, and a management Web UI for diskless LeapOS Device
testing.

DHCP is provided by the lab router. The NetBoot server does **not** run DHCP.

## Current Lab Target

All NetBoot server and PXE client work targets D945GSEJT boards:

- NetBoot server: D945GSEJT + Atom N270, CF/SD raw image, legacy BIOS.
- PXE clients/slaves: D945GSEJT boards booting the bundled Alpine LeapOS Device image.
- Lab server IP: `172.16.82.188`.
- Timezone: `America/Chicago`; the image runs BusyBox `ntpd` as an NTP client.

Use [`alpine-i386/`](alpine-i386/) for the server image. The older x86_64 image
builder is not used for the D945 lab.

## Features

- GRUB BIOS PXE loader at `boot/grub/i386-pc/core.0`.
- HTTP boot paths for kernel, initramfs, modloop, and apkovl files.
- Web UI + REST API for image catalog, default image, per-MAC device mapping, and friendly names.
- Bundled default `leapdevice001` Alpine LeapOS Device PXE image, seeded into the server image at build time.
- Per-device scripts under `ipxe/by-mac/` for optional iPXE use.
- OpenRC services for `nginx`, `in.tftpd`, `leap-netboot`, and `ntpd`.

## One-Shot Lab Build

Run from WSL:

```bash
cd /mnt/d/LEAP_Protocol/NetbootServer/alpine-i386
bash build-d945-lab.sh
```

This builds:

1. the static i386 `leap-device` binary,
2. the Alpine device PXE bundle (`vmlinuz`, `initramfs`, `modloop`, `leap-device.apkovl.tar.gz`),
3. the D945 NetBoot server image with that device bundle preloaded.

Output:

```text
platforms/x86-32/D945GSEJT/LeapOS/rtems-image/leap-netboot-server-d945.img
```

First-time rootfs cache creation may need `sudo` from your WSL terminal. Later
rebuilds normally reuse the cached rootfs and do not need root.

Host packages:

```bash
sudo apt install -y qemu-user-static binfmt-support wget tar rsync \
  util-linux e2fsprogs grub-pc-bin squashfs-tools gcc-multilib
```

## Flash and Boot

Flash the raw image to CF/SD:

```bash
sudo dd if=/mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapOS/rtems-image/leap-netboot-server-d945.img \
  of=/dev/sdX bs=4M status=progress conv=fsync
```

BIOS setup:

- Legacy boot.
- CF/SD first in boot order.
- Serial console COM1 at `115200 8N1`.
- Onboard Ethernet connected to the lab network.

The image auto-expands partition 1 and the ext4 root filesystem on first boot.
After DHCP, open:

```text
http://172.16.82.188/
```

Default services after boot:

| Service | Role |
|---------|------|
| `nginx` | Web UI on `:80`, HTTP boot under `/httpboot/` |
| `in.tftpd` | TFTP on UDP/69, root `/var/lib/leap-netboot/tftp` |
| `leap-netboot` | Management API on `127.0.0.1:8081` |
| `ntpd` | NTP client for server date/time |

## Router DHCP

Your router must pass PXE options pointing at this server — **do not run DHCP here**.
See [docs/ROUTER-DHCP.md](docs/ROUTER-DHCP.md).

Use the lab router/UniFi DHCP options:

| Option | Value |
|--------|--------|
| **66** (next-server / TFTP server) | `172.16.82.188` |
| **67** (boot filename) | `boot/grub/i386-pc/core.0` |

GRUB PXE is the default path for the D945 lab. iPXE remains available as an
optional second-stage path, but it is not required.

## Bundled Diskless LeapOS Device Image

The current default client image is the Alpine LeapOS Device PXE bundle:

```text
platforms/x86-32/D945GSEJT/LeapOS/rtems-image/leap-device-alpine-pxe.tar.gz
```

At build time, `build-d945-lab.sh` seeds this bundle into the NetBoot server as:

```text
image id: leapdevice001
name:     LeapOS device Alpine (PXE)
```

PXE clients load:

- `vmlinuz-lts`
- `initramfs-lts`
- `modloop-lts`
- `leap-device.apkovl.tar.gz`

The apkovl starts `/usr/sbin/leap-device` in the RAM root. No local disk is
required on the slave. This is the image used for diskless conformance testing.

## Optional RTEMS Payload Publishing

After building `leap-port.exe` in the LEAP repo:

```bash
cd NetbootServer/scripts
./publish-leapos-rtems.sh \
  ../../platforms/x86-32/D945GSEJT/LeapOS/rtems-image/leap-port.exe \
  --name "LeapOS device $(date +%Y%m%d)"
```

RTEMS PXE remains supported for experiments, but it is not the default lab
client path.

## Web UI

The Web UI is styled like the LeapOS Gateway console and provides:

- image catalog and default image selection,
- device registry with friendly names, notes, and per-MAC image overrides,
- settings including server label and PXE HTTP server IP,
- a Netbooted Devices report table for boot inventory data when available.

## Upload Troubleshooting

Alpine’s `/etc/nginx/nginx.conf` sets `client_max_body_size 1m` globally. The NetBoot
`leap-netboot.conf` server block overrides that to **512m** for uploads.

**On the running NetBoot server (serial/SSH):**

```sh
rm -f /etc/nginx/http.d/00-body-size.conf
/opt/leap-netboot/scripts/apply-nginx-upload-limits.sh
```

Do **not** add `http.d/00-body-size.conf` — it duplicates the http{} directive and
nginx will refuse to start.

Repack/reflash `alpine-i386` to bake the fix in permanently.

## Rebuild Only the Alpine Device PXE Bundle

After building the Alpine device rootfs and PXE tree:

```bash
cd platforms/x86-32/D945GSEJT/LeapDevice-linux/alpine
bash make-pxe-device-alpine.sh
```

This also creates `LeapOS/rtems-image/leap-device-alpine-pxe.tar.gz` for the Web UI.

Normally `build-d945-lab.sh` handles this. To rebuild only the PXE bundle:

```bash
cd /mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapDevice-linux
bash build-leap-device.sh
cd alpine
bash make-pxe-device-alpine.sh
```

Then rebuild/reflash the server image so the bundle is preseeded into the CF/SD image.

## Project layout

```
NetbootServer/
  README.md
  docs/                 Router DHCP + architecture
  alpine-i386/          D945GSEJT NetBoot server image builder
  server/               leap_netbootd (Python API)
  web/                  Static Web UI
  scripts/              publish + PXE render helpers
  config/               Example state files
```

## Related docs

- [platforms/x86-32/D945GSEJT/LeapOS/docs/PXE.md](../platforms/x86-32/D945GSEJT/LeapOS/docs/PXE.md) — LeapPort Multiboot payload notes
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — boot paths and state files
