#!/bin/bash
# Stage LeapOS-Device PXE boot tree (leap-port.exe + GRUB PXE loader).
#   bash make-pxe-device.sh              # use existing leap-port.exe
#   bash make-pxe-device.sh --build      # rebuild leap-port.exe first
#
# Output: rtems-image/pxe-device/  — copy to TFTP root or upload leap-port.exe
#         via NetbootServer/scripts/publish-leapos-rtems.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

DO_BUILD=0
PXE_PREFIX="${PXE_PREFIX:-leapos-device}"
PXE_OUT="${LEAPOS_PXE_DEVICE_DIR:-$LEAPOS_IMAGE_DIR/pxe-device}"
GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"

while [ $# -gt 0 ]; do
	case "$1" in
	--build) DO_BUILD=1; shift ;;
	-h|--help)
		echo "Usage: $0 [--build]" >&2
		exit 0
		;;
	*)
		echo "unknown option: $1" >&2
		exit 1
		;;
	esac
done

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "error: missing command: $1" >&2
		exit 1
	fi
}

grub_pick_mods() {
	local mod
	for mod in "$@"; do
		if [ -f "$GRUB_DIR/${mod}.mod" ]; then
			echo -n "$mod "
		fi
	done
}

need_cmd grub-mkimage cp rsync

if [ ! -f "$GRUB_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc not found at $GRUB_DIR" >&2
	echo "Install: sudo apt install -y grub-pc-bin" >&2
	exit 1
fi

if [ "$DO_BUILD" = "1" ] || [ ! -f "$LEAP_PORT_EXE" ]; then
	bash "$SCRIPT_DIR/build-leap-port.sh"
fi

if [ ! -f "$LEAP_PORT_EXE" ]; then
	echo "error: leap-port.exe not found at $LEAP_PORT_EXE" >&2
	exit 1
fi

echo "=== LeapOS-Device PXE staging ==="
echo "Output:  $PXE_OUT"
echo "Prefix:  /${PXE_PREFIX}/boot/grub"

rm -rf "$PXE_OUT"
mkdir -p "$PXE_OUT/boot/grub/i386-pc"

cp "$LEAP_PORT_EXE" "$PXE_OUT/leap-port.exe"
cp "$SCRIPT_DIR/grub/leapos-device-pxe-grub.cfg" "$PXE_OUT/boot/grub/grub.cfg"

pxe_mods="$(grub_pick_mods \
	pxe tftp net multiboot serial terminal gzio normal configfile echo \
	e1000 ne2k_pci rtl8139 rtl8168 ata pata)"
if [ -z "$pxe_mods" ]; then
	echo "error: no GRUB PXE modules under $GRUB_DIR" >&2
	exit 1
fi

# shellcheck disable=SC2086
grub-mkimage \
	-O i386-pc-pxe \
	-o "$PXE_OUT/boot/grub/i386-pc/core.0" \
	-d "$GRUB_DIR" \
	-p "/${PXE_PREFIX}/boot/grub" \
	$pxe_mods

cp "$GRUB_DIR"/*.mod "$PXE_OUT/boot/grub/i386-pc/" 2>/dev/null || true

ls -lh "$PXE_OUT/leap-port.exe" "$PXE_OUT/boot/grub/i386-pc/core.0"
du -sh "$PXE_OUT"

cat <<EOF

PXE tree ready: $PXE_OUT

Standalone TFTP (copy entire tree under /var/lib/tftpboot/):
  dhcp option 67: ${PXE_PREFIX}/boot/grub/i386-pc/core.0

LeapOS NetBoot Server (upload payload only):
  ../../NetbootServer/scripts/publish-leapos-rtems.sh \\
    $LEAP_PORT_EXE --name "LeapOS device" --default

See LeapOS/docs/PXE.md and NetbootServer/docs/ROUTER-DHCP.md
EOF
