# LeapOS-Device — Alpine i386 image

Device-only Linux image for D945GSEJT (Atom N270, SATA CF, RTL8111). Smaller
than the gateway image — no Web UI, OpENer, or `/cf` config persistence.

## Host setup (WSL Ubuntu, one-time)

```bash
sudo apt update
sudo apt install -y \
  qemu-user-static binfmt-support \
  wget tar gzip rsync \
  util-linux e2fsprogs grub-pc-bin gcc-multilib
sudo update-binfmts --enable qemu-i386 2>/dev/null || true
```

## Build

```bash
cd /mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapDevice-linux
bash build-leap-device.sh
cd alpine
sudo FORCE_ROOTFS=1 bash mk-image.sh    # first build, or after packages.txt changes
bash mk-image.sh                        # repack after overlay/binary changes
bash make-pxe-device-alpine.sh          # PXE tree (kernel + initramfs + modloop)
```

Output: `LeapOS/rtems-image/leapos-device-alpine.img`  
PXE output: `LeapOS/rtems-image/pxe-device-alpine/`

## Diskless PXE Boot

The active lab path is diskless PXE from the D945 NetBoot server. The generated
PXE bundle contains:

| File | Purpose |
|------|---------|
| `vmlinuz-lts` | Alpine i386 kernel |
| `initramfs-lts` | PXE-capable initramfs with networking/r8169 support |
| `modloop-lts` | Alpine module loop in official `modules/<kver>/` layout |
| `leap-device.apkovl.tar.gz` | RAM-root overlay with `leap-device`, hostname, networking, OpenRC runlevels |

`make-pxe-device-alpine.sh` also writes:

```text
LeapOS/rtems-image/leap-device-alpine-pxe.tar.gz
```

`NetbootServer/alpine-i386/build-d945-lab.sh` builds this bundle and preloads it
into the NetBoot server image as `leapdevice001`, so a freshly flashed server can
PXE-boot D945 slaves without a Web UI upload.

Expected client behavior:

- DHCP address from the lab router.
- `apkovl` loads from the NetBoot server.
- `modloop` mounts and `/lib/modules` exists.
- `leap-device` starts from `local.d` in the RAM root.
- Conformance Studio can target the discovered device by selected MAC.

## Boot

1. Flash image to CF (raw `.img`, not ISO)
2. BIOS: SATA/CF first, legacy boot, LBA enabled
3. Parallel port: SPP / bi-directional, base `0x378`
4. Serial **COM1 @ 115200 8N1**

Expected log line:

```text
LEAP full stack listening on eth0 (DISC/DIR/MGMT/PD/DIAG)
```

## vs RTEMS device

| | RTEMS (`leapos-device.img`) | Alpine (`leapos-device-alpine.img`) |
|--|------------------------------|-------------------------------------|
| Boot | GRUB multiboot → `leap-port.exe` | GRUB → Linux → `leap-device` |
| NIC | libbsd `re0` | kernel `r8169` → `eth0` |
| LPT | direct `inb`/`outb` | `iopl`/`ioperm` + `inb`/`outb` |
| Image size | ~128 MB FAT | ~160–245 MB ext4 (auto-sized; build verifies partition = ext4) |
| PXE-friendly | Yes (RAM-only) | Yes (kernel + initramfs + modloop + apkovl) |

Both run the same `leap_core` device stack and 8×8 LPT profile.

## GPIO / bus debug (on target)

`libgpiod` is included (`gpiodetect`, `gpioinfo`, `gpioget`, `gpioset`):

```sh
gpiodetect
gpioinfo gpiochip0
```

PCI and I2C helpers for board bring-up (`pciutils`, `i2c-tools`):

```sh
lspci -nn
i2cdetect -l
i2cdetect -y 0
```
