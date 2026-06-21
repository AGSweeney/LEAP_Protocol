#!/bin/bash
# Build Alpine LeapOS Device image with MCC PCI-DIO-24H board support.
#
# Output: LeapOS/rtems-image/leapos-device-alpine-mcc-dio24.img
_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_DIR="$(cd "$SCRIPT_DIR/../LeapOS/rtems-image" && pwd)"
export LEAP_BOARD=mcc_dio24
export LEAPOS_DEVICE_ALPINE_IMG="$IMAGE_DIR/leapos-device-alpine-mcc-dio24.img"

echo "=== LeapOS Device Alpine (MCC PCI-DIO-24H) ==="
bash "$SCRIPT_DIR/build-leap-device.sh"

if [ ! -d "$SCRIPT_DIR/alpine/work/rootfs" ] || [ "${FORCE_ROOTFS:-0}" = "1" ]; then
	echo "Building rootfs (requires sudo on first run)..."
	( cd "$SCRIPT_DIR/alpine" && sudo FORCE_ROOTFS=1 bash mk-image.sh )
else
	( cd "$SCRIPT_DIR/alpine" && bash mk-image.sh )
fi

echo ""
echo "MCC DIO24 test image ready:"
ls -lh "$LEAPOS_DEVICE_ALPINE_IMG"
echo ""
echo "QEMU smoke test:"
echo "  qemu-system-i386 -m 256 -drive file=$LEAPOS_DEVICE_ALPINE_IMG,format=raw,if=ide -serial mon:stdio -nographic"
