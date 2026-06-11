#!/bin/bash
# Build raw MBR + FAT32 disk image for D945GSEJT CF-via-IDE boot.
# No sudo required — uses mtools + grub-mkimage (same host tools as ISO build).
# Usage: make-cf-image.sh [device|gateway]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

PROFILE="${1:-device}"
IMAGE_MB="${IMAGE_MB:-128}"
PART_START=2048
PART_OFF=$((PART_START * 512))
GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"
GRUB_MODS="multiboot part_msdos biosdisk fat fshelp serial terminal gzio relocator"
WORK="$RTEMS_ROOT/build/cf-image-work-${PROFILE}"

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

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "error: missing command: $1" >&2
		echo "Install: apt install mtools dosfstools fdisk grub-pc-bin" >&2
		exit 1
	fi
}

need_cmd mformat
need_cmd mcopy
need_cmd sfdisk
need_cmd grub-mkimage
need_cmd truncate
need_cmd dd

if [ ! -d "$GRUB_DIR" ] || [ ! -f "$GRUB_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc files not found at $GRUB_DIR" >&2
	exit 1
fi

bash "$SCRIPT_DIR/stage-payload.sh" "$PROFILE"

if [ ! -f "$PAYLOAD" ]; then
	echo "error: payload not found: $PAYLOAD" >&2
	exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK/boot/grub/i386-pc"

rm -f "$IMG"
truncate -s "${IMAGE_MB}M" "$IMG"

echo "label: dos
unit: sectors
start=${PART_START}, size=-, type=c, bootable" | sfdisk "$IMG"

mformat -i "$IMG@@${PART_OFF}" -F -v LEAPOS -c 1
mmd -i "$IMG@@${PART_OFF}" ::boot ::boot/grub ::boot/grub/i386-pc

mcopy -i "$IMG@@${PART_OFF}" -s "$PAYLOAD" "::${PAYLOAD_NAME}"
mcopy -i "$IMG@@${PART_OFF}" -s "$GRUB_CFG" ::boot/grub/grub.cfg

if [ "$PROFILE" = "gateway" ]; then
	cat > "$WORK/config.txt" <<'EOF'
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
	mcopy -i "$IMG@@${PART_OFF}" -s "$WORK/config.txt" ::config.txt
fi

cat > "$WORK/early.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set root=(hd0,msdos1)
multiboot /${PAYLOAD_NAME} --video=off --console=/dev/com1,115200 --printk=/dev/com1,115200 --ide=0,1 --ide-show
boot
EOF

# shellcheck disable=SC2086
grub-mkimage -O i386-pc -o "$WORK/core.img" -d "$GRUB_DIR" \
	--prefix='(hd0,msdos1)/boot/grub' \
	-c "$WORK/early.cfg" \
	$GRUB_MODS

core_bytes="$(wc -c < "$WORK/core.img" | tr -d ' ')"
if [ "$core_bytes" -gt 491520 ]; then
	echo "error: GRUB core.img too large for MBR embed (${core_bytes} bytes)" >&2
	exit 1
fi

dd if="$GRUB_DIR/boot.img" of="$IMG" conv=notrunc bs=446 count=1 status=none
dd if="$WORK/core.img" of="$IMG" conv=notrunc bs=512 seek=1 status=none

bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$IMG"
echo "CF/IDE image ready ($PROFILE): $IMG"
echo "Flash to CF with dd (raw block write, not ISO):"
echo "  dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
