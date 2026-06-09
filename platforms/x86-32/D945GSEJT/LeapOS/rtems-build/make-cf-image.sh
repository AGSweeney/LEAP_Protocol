#!/bin/bash
# Build raw MBR + FAT32 disk image for D945GSEJT CF-via-IDE boot (recommended).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

IMAGE_MB="${IMAGE_MB:-128}"
IMG="$LEAPOS_IMAGE_DIR/leapos-rtems-poc.img"
WORK="$RTEMS_ROOT/build/cf-image-work"

if [ "$(id -u)" -ne 0 ]; then
    echo "This script needs loop mounts — re-run with sudo:" >&2
    echo "  sudo IMAGE_MB=$IMAGE_MB bash rtems-build/make-cf-image.sh" >&2
    exit 1
fi

bash "$SCRIPT_DIR/stage-payload.sh"

rm -rf "$WORK"
mkdir -p "$WORK/mnt"

rm -f "$IMG"
truncate -s "${IMAGE_MB}M" "$IMG"

# One primary FAT32 partition, bootable.
echo 'label: dos
unit: sectors
start=2048, size=-, type=c, bootable' | sfdisk "$IMG"

LOOP="$(losetup -fP --show "$IMG")"
cleanup() {
    mountpoint -q "$WORK/mnt" && umount "$WORK/mnt"
    [ -n "${LOOP:-}" ] && losetup -d "$LOOP" 2>/dev/null || true
}
trap cleanup EXIT

mkfs.vfat -F 32 -n LEAPOS "${LOOP}p1"
mount "${LOOP}p1" "$WORK/mnt"

mkdir -p "$WORK/mnt/boot/grub"
cp "$LEAPOS_STAGING/leap-port.exe" "$WORK/mnt/"
if [ -f "$LEAPOS_STAGING/net-probe.exe" ]; then
	cp "$LEAPOS_STAGING/net-probe.exe" "$WORK/mnt/"
fi
cp "$SCRIPT_DIR/grub/leapos-grub.cfg" "$WORK/mnt/boot/grub/grub.cfg"

grub-install \
    --target=i386-pc \
    --boot-directory="$WORK/mnt/boot" \
    --modules="multiboot part_msdos biosdisk normal serial terminal echo" \
    --force \
    "$IMG"

sync
umount "$WORK/mnt"
losetup -d "$LOOP"
LOOP=""
trap - EXIT

cp "$LEAP_PORT_EXE" "$LEAPOS_IMAGE_DIR/leap-port.exe"
if [ -f "$NET_PROBE_EXE" ]; then
	cp "$NET_PROBE_EXE" "$LEAPOS_IMAGE_DIR/net-probe.exe"
fi
bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$IMG" "$LEAPOS_IMAGE_DIR/leap-port.exe"
echo "CF/IDE image ready: $IMG"
echo "Flash with: sudo dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
