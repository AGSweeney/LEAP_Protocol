#!/bin/bash
# Build raw MBR + FAT boot + ext4 root Alpine aarch64 SD image for Pi 4 gateway.
# Requires root (aarch64 chroot on x86_64, or native on aarch64 host).
# Run: sudo bash mk-image.sh
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
	echo "error: run as root: sudo bash $0" >&2
	exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PI4_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_DIR="${LEAP_GATEWAY_IMAGE_DIR:-$PI4_ROOT/image}"
IMG="${LEAP_GATEWAY_ALPINE_IMG:-$IMAGE_DIR/leapos-gateway-alpine.img}"
CACHE="$SCRIPT_DIR/cache"
APK_CACHE="$CACHE/apk"

ALPINE_RELEASE="${ALPINE_RELEASE:-3.20.6}"
ALPINE_BRANCH="${ALPINE_BRANCH:-$(echo "$ALPINE_RELEASE" | cut -d. -f1,2)}"
IMAGE_MB="${IMAGE_MB:-512}"
BOOT_PART_MB="${BOOT_PART_MB:-128}"
BOOT_PART_START=8192
BOOT_PART_SECTORS=$((BOOT_PART_MB * 2048))
ROOT_PART_START=$((BOOT_PART_START + BOOT_PART_SECTORS))
WORK="$SCRIPT_DIR/build-work"
ROOT="$WORK/rootfs"
MINIROOT="$CACHE/alpine-minirootfs-${ALPINE_RELEASE}-aarch64.tar.gz"
MINIROOT_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_BRANCH}/releases/aarch64/alpine-minirootfs-${ALPINE_RELEASE}-aarch64.tar.gz"
ROOTFS_STAMP="$ROOT/.leap-alpine-rootfs-ready"
PKG_HASH_FILE="$ROOT/.leap-alpine-packages-hash"
HOST_ARCH="$(uname -m)"

need_rootfs_rebuild() {
	if [ "${FORCE_ROOTFS:-0}" = "1" ]; then
		return 0
	fi
	if [ ! -f "$ROOTFS_STAMP" ] || [ ! -d "$ROOT/etc/apk" ]; then
		return 0
	fi
	_pkg_hash="$(md5sum "$SCRIPT_DIR/packages.txt" | awk '{print $1}')"
	if [ ! -f "$PKG_HASH_FILE" ] || [ "$(cat "$PKG_HASH_FILE")" != "$_pkg_hash" ]; then
		return 0
	fi
	return 1
}

unmount_chroot_mounts() {
	local root="${1:-$ROOT}"
	[ -d "$root" ] || return 0
	umount "$root/var/cache/apk" 2>/dev/null || true
	umount "$root/dev/pts" 2>/dev/null || true
	umount "$root/dev/shm" 2>/dev/null || true
	umount "$root/dev" 2>/dev/null || true
	umount "$root/sys" 2>/dev/null || true
	umount "$root/proc" 2>/dev/null || true
}

strip_overlay_text() {
	find "$ROOT/etc" "$ROOT/usr/sbin" "$ROOT/cf" "$ROOT/boot" -type f \
		-exec sed -i 's/\r$//' {} + 2>/dev/null || true
}

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "error: missing command: $1" >&2
		echo "Install: sudo apt install -y qemu-user-static binfmt-support wget tar util-linux rsync genext2fs mtools" >&2
		exit 1
	fi
}

need_cmd wget tar sfdisk rsync chroot genext2fs dd mformat mcopy truncate

if [ "$HOST_ARCH" != "aarch64" ] && [ "$HOST_ARCH" != "arm64" ]; then
	QEMU="$(command -v qemu-aarch64-static || true)"
	if [ -z "$QEMU" ]; then
		echo "error: qemu-aarch64-static not found (needed for aarch64 chroot on $HOST_ARCH)" >&2
		exit 1
	fi
else
	QEMU=""
fi

trim_rootfs() {
	echo "Trimming rootfs (drop unused firmware and kernel modules) ..."
	chroot "$ROOT" /bin/sh <<'TRIM'
set -e
rm -rf /usr/share/man /usr/share/doc /var/cache/apk/*
rm -f /boot/modloop-* 2>/dev/null || true
KVER="$(ls -1 /lib/modules 2>/dev/null | grep -v -E 'microsoft|WSL' | head -1)"
if [ -z "$KVER" ] || [ ! -d "/lib/modules/$KVER" ]; then
	echo "error: kernel modules not found under /lib/modules" >&2
	ls -la /lib/modules >&2 || true
	exit 1
fi
keep_ko() {
	case "$1" in
	*/kernel/drivers/net/ethernet/broadcom/genet/*|\
	*/kernel/drivers/net/ethernet/broadcom/bcmgenet.ko*|\
	*/kernel/drivers/net/mdio/*|*/kernel/drivers/net/phy/*|\
	*/kernel/drivers/mmc/*|*/kernel/drivers/block/mmcblk.ko*|\
	*/kernel/drivers/usb/storage/*|\
	*/kernel/fs/ext4/*|*/kernel/lib/crc/*|\
	*/kernel/drivers/soc/bcm/*|*/kernel/drivers/clk/bcm/*|\
	*/kernel/drivers/pinctrl/bcm/*|*/kernel/drivers/gpio/gpio-bcm*/*) return 0 ;;
	esac
	return 1
}
find "/lib/modules/$KVER" \( -name '*.ko' -o -name '*.ko.xz' \) | while read -r ko; do
	keep_ko "$ko" || rm -f "$ko"
done
TRIM
}

build_rootfs() {
	mkdir -p "$CACHE" "$APK_CACHE" "$ROOT"

	if [ ! -f "$MINIROOT" ] || [ ! -s "$MINIROOT" ]; then
		rm -f "$MINIROOT"
		echo "Downloading minirootfs (once, cached under cache/) ..."
		wget -O "$MINIROOT" "$MINIROOT_URL"
	else
		echo "Using cached minirootfs: $MINIROOT"
	fi
	if [ ! -s "$MINIROOT" ]; then
		echo "error: download failed or empty: $MINIROOT" >&2
		exit 1
	fi

	unmount_chroot_mounts "$ROOT"
	rm -rf "$ROOT"
	mkdir -p "$ROOT"
	echo "Extracting minirootfs ..."
	tar -xzf "$MINIROOT" -C "$ROOT"
	if [ -n "$QEMU" ]; then
		install -m 755 "$QEMU" "$ROOT/usr/bin/qemu-aarch64-static"
	fi

	echo "Installing packages in aarch64 chroot (apk cache: $APK_CACHE) ..."
	_pkg_list="$WORK/apk-packages.lst"
	sed 's/\r$//' "$SCRIPT_DIR/packages.txt" |
		grep -v '^#' |
		grep -v '^[[:space:]]*$' > "$_pkg_list"
	mapfile -t _apk_pkgs < "$_pkg_list"
	if [ "${#_apk_pkgs[@]}" -eq 0 ]; then
		echo "error: no packages in $SCRIPT_DIR/packages.txt" >&2
		exit 1
	fi

	cp /etc/resolv.conf "$ROOT/etc/resolv.conf"
	mkdir -p "$ROOT/var/cache/apk"
	mount -t proc proc "$ROOT/proc"
	mount -t sysfs sys "$ROOT/sys"
	mount --bind /dev "$ROOT/dev"
	mount --bind "$APK_CACHE" "$ROOT/var/cache/apk"

	chroot "$ROOT" /sbin/apk update
	chroot "$ROOT" /sbin/apk add "${_apk_pkgs[@]}"

	echo "Applying overlay ..."
	rsync -a "$SCRIPT_DIR/overlay/" "$ROOT/"
	strip_overlay_text
	chmod 755 "$ROOT/usr/sbin/leap-gateway" "$ROOT/etc/init.d/leap-gateway"

	trim_rootfs
	md5sum "$SCRIPT_DIR/packages.txt" | awk '{print $1}' > "$PKG_HASH_FILE"
	touch "$ROOTFS_STAMP"

	unmount_chroot_mounts "$ROOT"
	if [ -n "$QEMU" ]; then
		rm -f "$ROOT/usr/bin/qemu-aarch64-static"
	fi
}

enable_svc() {
	local runlevel="$1" svc="$2"
	if [ ! -e "$ROOT/etc/init.d/$svc" ]; then
		echo "warn: init script missing, skipping: $svc" >&2
		return 0
	fi
	mkdir -p "$ROOT/etc/runlevels/$runlevel"
	ln -snf "/etc/init.d/$svc" "$ROOT/etc/runlevels/$runlevel/$svc"
}

write_boot_config() {
	local cmdline="console=serial0,115200 console=tty1 root=LABEL=LEAPGW rootfstype=ext4 rootwait modules=sd-mod,usb-storage,ext4"
	mkdir -p "$ROOT/boot"
	printf '%s\n' "$cmdline" > "$ROOT/boot/cmdline.txt"
	if [ -f "$ROOT/boot/config.txt" ] && [ -f "$ROOT/boot/leap-config.txt" ]; then
		echo "" >> "$ROOT/boot/config.txt"
		cat "$ROOT/boot/leap-config.txt" >> "$ROOT/boot/config.txt"
	elif [ -f "$ROOT/boot/leap-config.txt" ]; then
		cp "$ROOT/boot/leap-config.txt" "$ROOT/boot/config.txt"
	fi
	rm -f "$ROOT/boot/leap-config.txt"
}

pack_boot_partition() {
	local boot_img="$1"
	local boot_bytes=$((BOOT_PART_MB * 1024 * 1024))
	if [ ! -d "$ROOT/boot" ] || [ -z "$(ls -A "$ROOT/boot" 2>/dev/null)" ]; then
		echo "error: /boot is empty — linux-rpi / raspberrypi-bootloader may have failed" >&2
		exit 1
	fi
	truncate -s "${boot_bytes}" "$boot_img"
	mformat -F -v LEAPBOOT -i "$boot_img" ::
	mcopy -i "$boot_img" -s "$ROOT/boot"/* ::/
}

unmount_chroot_mounts "$ROOT"
trap 'unmount_chroot_mounts "$ROOT"' EXIT

echo "=== LeapOS-Gateway Alpine image build (Pi 4) ==="
echo "Release: Alpine ${ALPINE_RELEASE} aarch64"
echo "Output:  $IMG (${IMAGE_MB} MiB, boot ${BOOT_PART_MB} MiB)"
echo "Host:    ${HOST_ARCH}${QEMU:+ (qemu-user chroot)}"

if need_rootfs_rebuild; then
	build_rootfs
else
	echo "Reusing cached rootfs ($ROOT) — only repacking disk image"
	echo "  (set FORCE_ROOTFS=1 to re-run apk/chroot from scratch)"
fi

ROOT_KB="$(du -sk "$ROOT" | awk '{print $1}')"
echo "Rootfs size: $((ROOT_KB / 1024)) MiB"

IMAGE_BYTES=$((IMAGE_MB * 1024 * 1024))
IMAGE_SECTORS=$((IMAGE_BYTES / 512))
ROOT_PART_SECTORS=$((IMAGE_SECTORS - ROOT_PART_START))
ROOT_PART_BYTES=$((ROOT_PART_SECTORS * 512))
ROOT_PART_KB=$((ROOT_PART_BYTES / 1024))
ROOT_PART_MIB=$((ROOT_PART_KB / 1024))

if [ "$ROOT_KB" -gt $((ROOT_PART_KB * 95 / 100)) ]; then
	echo "error: rootfs ($((ROOT_KB / 1024)) MiB) too large for ${ROOT_PART_MIB} MiB root partition" >&2
	echo "Increase IMAGE_MB or trim packages/modules." >&2
	du -sh "$ROOT"/* 2>/dev/null | sort -h | tail -8 >&2
	exit 1
fi

echo "Creating ${IMAGE_MB} MiB disk image ..."
mkdir -p "$IMAGE_DIR"
rm -f "$IMG"
truncate -s "${IMAGE_MB}M" "$IMG"

echo "label: dos
unit: sectors
start=${BOOT_PART_START}, size=${BOOT_PART_SECTORS}, type=c, bootable
start=${ROOT_PART_START}, size=-, type=83" | sfdisk "$IMG"

LINUX_WORK="${ALPINE_LINUX_WORK:-/tmp/leap-alpine-pi4-build}"
PART_IMG="$LINUX_WORK/partition.ext2"
BOOT_IMG="$LINUX_WORK/bootpart.fat"
mkdir -p "$LINUX_WORK"

echo "Applying boot fixups ..."
sed -i 's/^root:[^:]*:/root::/' "$ROOT/etc/shadow"
rsync -a "$SCRIPT_DIR/overlay/" "$ROOT/"
strip_overlay_text
chmod 755 "$ROOT/usr/sbin/leap-gateway" "$ROOT/etc/init.d/leap-gateway"

enable_svc sysinit devfs
enable_svc sysinit dmesg
enable_svc sysinit mdev
enable_svc sysinit hwdrivers
enable_svc boot fsck
enable_svc boot root
enable_svc boot localmount
enable_svc boot modules
enable_svc boot sysctl
enable_svc boot hostname
enable_svc boot bootmisc
enable_svc boot syslog
enable_svc default leap-gateway
rm -f "$ROOT/etc/runlevels/default/networking"
enable_svc shutdown killprocs
enable_svc shutdown savecache
enable_svc shutdown mount-ro

write_boot_config

FS_BLOCKS=$((ROOT_PART_BYTES / 4096))
echo "Packing rootfs with genext2fs (${ROOT_PART_MIB} MiB, staging on ${LINUX_WORK}) ..."
rm -f "$PART_IMG"
genext2fs -B 4096 -b "$FS_BLOCKS" -N "$FS_BLOCKS" -d "$ROOT" -L LEAPGW "$PART_IMG"

echo "Writing ext4 root partition (seek sector ${ROOT_PART_START}) ..."
dd if="$PART_IMG" of="$IMG" bs=512 seek="$ROOT_PART_START" conv=notrunc status=none

echo "Packing FAT boot partition (${BOOT_PART_MB} MiB) ..."
pack_boot_partition "$BOOT_IMG"
dd if="$BOOT_IMG" of="$IMG" bs=512 seek="$BOOT_PART_START" conv=notrunc status=none

ls -lh "$IMG"
echo ""
echo "Done: $IMG"
echo "Flash with Raspberry Pi Imager (Use custom) or:"
echo "  dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
echo "Serial: GPIO UART (serial0) 115200 8N1"
