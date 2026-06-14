# LeapOS NetBoot Server — Alpine i386 for D945GSEJT

NetBoot stack (nginx, TFTP, Web UI, `leap_netbootd`) built for
**Intel D945GSEJT + Atom N270**:

- Alpine **3.20.x i386** (not x86_64)
- **MBR + ext4 + GRUB i386-pc** (legacy BIOS / CF — same layout as LeapDevice-linux)
- Serial console **COM1 @ 115200**
- Timezone **America/Chicago** with BusyBox `ntpd` enabled as an NTP client
- Bundled diskless Alpine LeapOS Device PXE image preloaded as the default image

## Build (WSL — run in **your** terminal)

Use the one-shot lab build when preparing a fresh D945 NetBoot server. It builds
the `leap-device` binary, creates the diskless Alpine PXE bundle, and preseeds
that bundle into the server image.

```bash
sudo apt install -y qemu-user-static binfmt-support wget tar rsync \
  util-linux e2fsprogs grub-pc-bin squashfs-tools gcc-multilib

cd NetbootServer/alpine-i386
bash build-d945-lab.sh
```

First-time rootfs cache creation may need **`sudo`** from your WSL terminal. For
server-only repacks after overlay/script changes, run `bash mk-image.sh`.

Output: `platforms/x86-32/D945GSEJT/LeapOS/rtems-image/leap-netboot-server-d945.img`

`IMAGE_MB=auto` builds a compact image (`rootfs + 32 MiB`, minimum 160 MiB). Flash it to
a larger CF/SD — on **first boot**, `leap-growfs` expands partition 1 and the ext4 root
filesystem to fill the physical disk, then writes `/var/lib/leap-growfs.done`.

After adding `e2fsprogs-extra` / `leap-growfs`, rebuild rootfs once:

```bash
sudo FORCE_ROOTFS=1 bash mk-image.sh
```

## Boot

1. Flash raw `.img` to CF (SATA/IDE on D945GSEJT)
2. BIOS: CF/IDE first, legacy boot, LBA on
3. Serial **COM1 115200 8N1** — expect OpenRC + nginx + leap-netboot + ntpd
4. `eth0` DHCP — Web UI at `http://<ip>/`

## Publish LeapOS Device Images

The normal path is already handled by `build-d945-lab.sh`. If you rebuild only
the device PXE tree and want to publish it to a running server manually:

```bash
# Copy publish script + local-data to the running server, or run on-server:
./publish-leapos-device-alpine.sh /path/to/pxe-device-alpine --default
```

Router DHCP options **66/67** point test D945 units at this server's IP.
