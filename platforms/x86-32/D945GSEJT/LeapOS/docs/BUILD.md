# LeapOS build guide

Build **bootable RTEMS 6.2 images** for Intel **D945GSEJT + Atom N270** from WSL2
(Ubuntu) or native Linux. Output artifacts land in `rtems-image/`.

## What you get

| Artifact | Use |
| --- | --- |
| `leapos-rtems-poc.img` | **Preferred for CF-via-IDE** — raw MBR + FAT32 + GRUB |
| `leapos-rtems-poc.iso` | USB boot or Etcher DD to CF |
| `leap-port.exe` | LEAP device ELF (default boot payload) |
| `net-probe.exe` | Network bring-up probe ELF (QEMU / manual GRUB) |

Both images boot **GRUB → LeapOS `leap-port.exe`** on the **pc386** BSP (i386, not x86-64).

## Prerequisites

### Host (WSL2 Ubuntu recommended on Windows)

```bash
sudo apt update
sudo apt install -y \
  build-essential python3 python3-venv wget tar bzip2 \
  grub-pc-bin grub-common xorriso dosfstools fdisk util-linux \
  qemu-system-x86
```

There is **no** `python3-rtems` package on Ubuntu Noble — the RTEMS Source Builder
(RSB) installs the cross toolchain under `~/rtems/6/`.

### One-time RTEMS toolchain

From the repo (WSL paths shown):

```bash
cd /mnt/d/LEAP_Protocol/platforms/x86-32/D945GSEJT/LeapOS/rtems-build
sed -i 's/\r$//' *.sh

# Fetch RTEMS 6.2 source tarballs into ~/rtems/src
bash setup-rtems-tree.sh

# Build i386-rtems6 + pc386 BSP
bash rsb-build.sh
```

RSB log: `~/rtems/build/rsb-i386.log`  
Install prefix: `~/rtems/6/`  
Cross compiler: `~/rtems/6/bin/i386-rtems6-gcc`

## Build boot images

### Full pipeline (leap-port + ISO + CF image)

```bash
cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
bash build-all.sh
```

From **Windows PowerShell** (same repo checkout):

```powershell
powershell -ExecutionPolicy Bypass -File platforms/x86-32/D945GSEJT/LeapOS/rtems-build/build-all.ps1
```

### Partial builds

```bash
bash build-all.sh leap-port   # leap-port.exe only
bash build-all.sh net-probe   # net-probe.exe only
bash build-all.sh iso         # hybrid ISO only
bash build-all.sh cf          # CF raw .img only (uses sudo for loop mount)
bash build-runonce.sh         # net-probe run-once (halts, no reboot loop) + ISO
```

Environment overrides (optional):

```bash
export RTEMS_ROOT=$HOME/rtems
export IMAGE_MB=128          # CF image size (default 128 MiB)
export RTEMS_BSP=pc386/pc386
```

## Flash and boot

See `rtems-image/README.txt` after each build.

**D945GSEJT lab setup:**

1. Flash **`leapos-rtems-poc.img`** to CF (preferred) or ISO via Etcher DD mode.
2. BIOS: CF/IDE first boot, legacy BIOS, LBA on, disable TCO watchdog if available.
3. Serial **COM1 @ 115200 8N1** — GRUB and RTEMS log on serial (VGA unreliable on 945GSE).
4. Select **LeapOS Device (serial COM1 @ 115200)** in GRUB.

Expected success:

```text
=== LeapOS booting (D945GSEJT / Atom N270) ===
*** LeapOS LEAP device (D945GSEJT, LPT 8x8 I/O) ***
LEAP full stack listening on re0 (DISC/DIR/MGMT/PD/DIAG)
```

## Script reference

| Script | Purpose |
| --- | --- |
| `env.sh` | Shared paths (`RTEMS_ROOT`, `LEAPOS_IMAGE_DIR`, …) |
| `check-deps.sh` | Verify apt packages + RTEMS toolchain |
| `setup-rtems-tree.sh` | Download RTEMS 6.2 tarballs to `~/rtems/src` |
| `rsb-build.sh` | Run RSB `6/rtems-i386` set |
| `build-leap-port.sh` | Build `leap-port.exe` (LEAP device) in rtems-libbsd |
| `build-net-probe.sh` | Build `net-probe.exe` (network bring-up probe) |
| `build-net.sh` | LEAP device ISO (leap-port + run-once BSP) |
| `stage-payload.sh` | Copy payload + GRUB cfg to staging |
| `make-boot-image.sh` | Produce `leapos-rtems-poc.iso` |
| `make-cf-image.sh` | Produce `leapos-rtems-poc.img` (MBR+FAT32) |
| `build-all.sh` | Orchestrate full pipeline |
| `build-runonce.sh` | BSP `BSP_RESET_BOARD_AT_EXIT=0` + net-probe + ISO |
| `build-all.ps1` | Windows → WSL wrapper |
| `grub/leapos-grub.cfg` | Serial-first GRUB menu for D945GSEJT |

## QEMU smoke test (host)

Does **not** prove D945GSEJT hardware boot — use after every build as a sanity check:

```bash
cd rtems-image
qemu-system-i386 -m 128 -no-reboot -nographic \
  -append "--video=off --console=/dev/com1,115200" -kernel net-probe.exe
```

## Troubleshooting

| Issue | Fix |
| --- | --- |
| `$'\r': command not found` | `sed -i 's/\r$//' rtems-build/*.sh` |
| `leap-port.exe not produced` | Ensure libbsd is built (`bash setup-libbsd.sh`) then `bash build-leap-port.sh` |
| `i386-rtems6-gcc not found` | Complete `bash rsb-build.sh` |
| CF boots GRUB but RTEMS hangs at `i386: isr=0 irr=1` | See [HARDWARE.md](HARDWARE.md) — serial capture, TCO watchdog, try `.img` not ISO |
| RSB fails on Noble | Install apt packages above; check `~/rtems/build/rsb-i386.log` |
| `grub-mkrescue` missing | `sudo apt install grub-pc-bin xorriso` |

## Relationship to LeapPort

| Tree | Role |
| --- | --- |
| **LeapOS** (`LeapOS/`) | RTEMS bring-up, boot media, GRUB payload staging |
| **LeapPort** (`LeapPort/`) | LEAP `leap_device_stack` application (built into the boot image as `leap-port.exe`) |

`build-leap-port.sh` stages the `LeapPort/` sources into the rtems-libbsd tree, builds
`leap-port.exe`, and the image scripts make it the default GRUB payload.

## Paths (default)

| What | Path |
| --- | --- |
| Repo (this tree) | `platforms/x86-32/D945GSEJT/LeapOS/` |
| WSL RTEMS root | `~/rtems/` |
| Toolchain prefix | `~/rtems/6/` |
| leap-port.exe (built) | `LeapOS/rtems-image/leap-port.exe` |
| Boot output | `LeapOS/rtems-image/` |
