#!/bin/bash
# Build raw MBR + FAT32 disk image for D945GSEJT CF-via-IDE boot.
# Usage: make-cf-image.sh [device|gateway]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

PROFILE="${1:-device}"
IMAGE_MB="${IMAGE_MB:-128}"
WORK="$RTEMS_ROOT/build/cf-image-work-${PROFILE}"

if [ "$(id -u)" -ne 0 ]; then
    echo "This script needs loop mounts — re-run with sudo:" >&2
    echo "  sudo IMAGE_MB=$IMAGE_MB bash rtems-build/make-cf-image.sh $PROFILE" >&2
    exit 1
fi

case "$PROFILE" in
device)
	IMG="$LEAPOS_DEVICE_IMG"
	GRUB_CFG="$SCRIPT_DIR/grub/leapos-device-grub.cfg"
	PAYLOAD="$LEAPOS_DEVICE_STAGING/leap-port.exe"
	PAYLOAD_NAME="leap-port.exe"
	;;
gateway)
	IMG="$LEAPOS_GATEWAY_IMG"
	GRUB_CFG="$SCRIPT_DIR/grub/leapos-gateway-grub.cfg"
	PAYLOAD="$LEAPOS_GATEWAY_STAGING/leap-eip-gateway.exe"
	PAYLOAD_NAME="leap-eip-gateway.exe"
	;;
*)
	echo "Usage: $0 [device|gateway]" >&2
	exit 1
	;;
esac

bash "$SCRIPT_DIR/stage-payload.sh" "$PROFILE"

rm -rf "$WORK"
mkdir -p "$WORK/mnt"

rm -f "$IMG"
truncate -s "${IMAGE_MB}M" "$IMG"

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
cp "$PAYLOAD" "$WORK/mnt/$PAYLOAD_NAME"
cp "$GRUB_CFG" "$WORK/mnt/boot/grub/grub.cfg"

cat > "$WORK/mnt/config.txt" <<'EOF'
# LeapOS-Gateway configuration (editable via Web UI Save to disk)
network.mode=single
network.ifname=re0
network.ipv4=192.168.1.2
network.mask=255.255.255.0
network.dhcp=0
cyclic_ms=50
mapping.begin=0
mapping.enabled=0
mapping.mac=00:00:00:00:00:00
mapping.profile=0x00010001
mapping.input.byte=0
mapping.input.bit=0
mapping.input.width=8
mapping.output.byte=2
mapping.output.bit=0
mapping.output.width=8
mapping.status.byte=4
EOF

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

bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$IMG"
echo "CF/IDE image ready ($PROFILE): $IMG"
echo "Flash with: sudo dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
