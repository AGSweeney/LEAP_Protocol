#!/bin/bash
# One-shot lab build: LeapOS device PXE bundle + D945 netboot server CF image.
# Run in WSL (sudo required once per rootfs cache).
#
#   bash build-d945-lab.sh
_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NETBOOT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$NETBOOT_ROOT/.." && pwd)"
DEVICE_ALPINE="$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapDevice-linux/alpine"
DEVICE_ROOT="$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapDevice-linux"
IMAGE_DIR="$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapOS/rtems-image"
BUNDLE="$IMAGE_DIR/leap-device-alpine-pxe.tar.gz"
SERVER_IMG="$IMAGE_DIR/leap-netboot-server-d945.img"

run_as_root() {
	if [ "$(id -u)" -eq 0 ]; then
		"$@"
	else
		sudo "$@"
	fi
}

echo "=== 1/3 LEAP device binary ==="
bash "$DEVICE_ROOT/build-leap-device.sh"

echo "=== 2/3 Device PXE bundle (initramfs + modloop + apkovl) ==="
if [ ! -f "$DEVICE_ALPINE/build-work/rootfs/.leap-alpine-rootfs-ready" ]; then
	echo "First device rootfs build needs root ..."
	run_as_root env FORCE_ROOTFS=1 bash "$DEVICE_ALPINE/mk-image.sh"
else
	echo "Reusing device rootfs cache"
fi
bash "$DEVICE_ALPINE/make-pxe-device-alpine.sh"
if [ ! -f "$BUNDLE" ]; then
	echo "error: expected bundle at $BUNDLE" >&2
	exit 1
fi
for entry in \
	'etc/.default_boot_services' \
	'etc/runlevels/sysinit/modloop' \
	'etc/runlevels/boot/networking' \
	'etc/runlevels/default/local' \
	'etc/local.d/leap-device.start'; do
	if ! tar -tzf "$IMAGE_DIR/pxe-device-alpine/leap-device.apkovl.tar.gz" |
		sed 's#^\./##' | grep -qx "$entry"; then
		echo "error: device apkovl missing $entry" >&2
		exit 1
	fi
done
ls -lh "$BUNDLE"

echo "=== 3/3 Netboot server CF image (bundled device pre-seeded) ==="
if [ ! -f "$SCRIPT_DIR/build-work/rootfs/.leap-netboot-i386-rootfs-ready" ]; then
	echo "First server rootfs build needs root ..."
	run_as_root env FORCE_ROOTFS=1 bash "$SCRIPT_DIR/mk-image.sh"
else
	bash "$SCRIPT_DIR/mk-image.sh"
fi

echo ""
echo "Done."
echo "  Flash: dd if=$SERVER_IMG of=/dev/sdX bs=4M status=progress conv=fsync"
echo "  UniFi: boot server = <this board IP>, filename = boot/grub/i386-pc/core.0"
echo "  Wait ~30s after power-on for DHCP + PXE menu regen, then boot slaves."
