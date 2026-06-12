#!/bin/bash
# Build raw MBR + ext4 Alpine i386 disk image for D945GSEJT gateway-only CF.
# Root is only required for the first apk/chroot rootfs build. When
# build-work/rootfs/.leap-alpine-rootfs-ready exists, repack is loop-free
# (mke2fs -d + GRUB) and runs without sudo.
#   bash mk-image.sh              # repack from cached rootfs
#   sudo FORCE_ROOTFS=1 bash mk-image.sh   # full rootfs rebuild
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LEAPOS_ROOT="$(cd "$SCRIPT_DIR/../../LeapOS" && pwd)"
IMAGE_DIR="${LEAPOS_IMAGE_DIR:-$LEAPOS_ROOT/rtems-image}"
IMG="${LEAPOS_GATEWAY_ALPINE_IMG:-$IMAGE_DIR/leapos-gateway-alpine.img}"
CACHE="$SCRIPT_DIR/cache"
APK_CACHE="$CACHE/apk"

ALPINE_RELEASE="${ALPINE_RELEASE:-3.20.6}"
ALPINE_BRANCH="${ALPINE_BRANCH:-$(echo "$ALPINE_RELEASE" | cut -d. -f1,2)}"
# auto = rootfs size + IMAGE_MARGIN_MB, rounded up. Override with IMAGE_MB=240.
IMAGE_MB="${IMAGE_MB:-auto}"
IMAGE_MIN_MB="${IMAGE_MIN_MB:-160}"
IMAGE_MARGIN_MB="${IMAGE_MARGIN_MB:-32}"
PART_START=2048
WORK="$SCRIPT_DIR/build-work"
ROOT="$WORK/rootfs"
MINIROOT="$CACHE/alpine-minirootfs-${ALPINE_RELEASE}-x86.tar.gz"
MINIROOT_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_BRANCH}/releases/x86/alpine-minirootfs-${ALPINE_RELEASE}-x86.tar.gz"
ROOTFS_STAMP="$ROOT/.leap-alpine-rootfs-ready"
PKG_HASH_FILE="$ROOT/.leap-alpine-packages-hash"

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

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "error: missing command: $1" >&2
		echo "Install: sudo apt install -y qemu-user-static binfmt-support wget tar util-linux rsync e2fsprogs grub-pc-bin" >&2
		exit 1
	fi
}

# No loop devices / mounts needed for image assembly:
#   mke2fs -d packs the rootfs into an ext4 partition file in userspace,
#   GRUB boot.img + core.img are dd'd raw (same method as RTEMS make-cf-image.sh).
# This is required on WSL2, whose kernel has no loop module.
need_cmd wget tar sfdisk rsync chroot mke2fs grub-mkimage dd

GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"
GRUB_MODS="biosdisk part_msdos ext2 linux serial terminal echo gzio"
if [ ! -f "$GRUB_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc files not found at $GRUB_DIR" >&2
	echo "Install: sudo apt install -y grub-pc-bin" >&2
	exit 1
fi

QEMU="$(command -v qemu-i386-static || true)"
if [ -z "$QEMU" ]; then
	echo "error: qemu-i386-static not found (needed for i386 chroot on x86_64 host)" >&2
	exit 1
fi

trim_rootfs() {
	echo "Trimming rootfs (drop firmware, modloop, unneeded modules) ..."
	chroot "$ROOT" /bin/sh <<'TRIM'
set -e
# Keep rtl_nic (RTL8111 firmware, ~1 MB) — drop all other firmware.
find /lib/firmware -mindepth 1 -maxdepth 1 ! -name rtl_nic -exec rm -rf {} + 2>/dev/null || true
rm -rf /usr/share/man /usr/share/doc /var/cache/apk/*
rm -f /boot/modloop-* 2>/dev/null || true
KVER="$(ls -1 /lib/modules 2>/dev/null | grep -v -E 'microsoft|WSL' | head -1)"
if [ -z "$KVER" ] || [ ! -d "/lib/modules/$KVER" ]; then
	echo "error: Alpine kernel modules not found under /lib/modules" >&2
	ls -la /lib/modules >&2 || true
	exit 1
fi
keep_ko() {
	case "$1" in
	*/kernel/drivers/ata/*|*/kernel/drivers/scsi/*|*/kernel/drivers/block/loop.ko*|\
	*/kernel/drivers/block/loop.ko.xz|*/kernel/drivers/net/r8169.ko*|\
	*/kernel/drivers/net/r8169.ko.xz|*/kernel/drivers/net/ethernet/realtek/*|\
	*/kernel/drivers/net/phy/*|*/kernel/drivers/net/mdio/*|\
	*/kernel/drivers/net/ethernet/intel/e1000/*|\
	*/kernel/fs/ext4/*|*/kernel/fs/squashfs/*|*/kernel/lib/crc/*) return 0 ;;
	esac
	return 1
}
find "/lib/modules/$KVER" \( -name '*.ko' -o -name '*.ko.xz' \) | while read -r ko; do
	keep_ko "$ko" || rm -f "$ko"
done
# Initramfs already built by apk (linux-lts trigger). Do not re-run depmod/mkinitfs
# in WSL chroot — they call uname -r and pick the host WSL2 kernel.
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
	install -m 755 "$QEMU" "$ROOT/usr/bin/qemu-i386-static"

	echo "Installing packages in i386 chroot (apk cache: $APK_CACHE) ..."
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
	# CRLF fixup for scripts/configs edited on Windows — grep -I skips ELF
	# binaries (sed would strip \r bytes inside them and corrupt the ELF).
	find "$ROOT/etc" "$ROOT/usr/sbin" -type f \
		-exec grep -Iq . {} \; -exec sed -i 's/\r$//' {} \; 2>/dev/null || true
	chmod 755 "$ROOT/usr/sbin/leap-gateway"
	# Service enablement happens at pack time (enable_svc) — rc-update in a
	# qemu-user chroot fails silently, so don't rely on it here.

	trim_rootfs
	md5sum "$SCRIPT_DIR/packages.txt" | awk '{print $1}' > "$PKG_HASH_FILE"
	touch "$ROOTFS_STAMP"

	unmount_chroot_mounts "$ROOT"
}

unmount_chroot_mounts "$ROOT"
trap 'unmount_chroot_mounts "$ROOT"' EXIT

echo "=== LeapOS-Gateway Alpine image build ==="
echo "Release: Alpine ${ALPINE_RELEASE} x86"
echo "Output:  $IMG (${IMAGE_MB} MiB)"

if need_rootfs_rebuild; then
	if [ "$(id -u)" -ne 0 ]; then
		echo "error: rootfs build needs root: sudo FORCE_ROOTFS=1 bash $0" >&2
		exit 1
	fi
	build_rootfs
else
	echo "Reusing cached rootfs ($ROOT) — only repacking disk image"
	echo "  (set FORCE_ROOTFS=1 to re-run apk/chroot from scratch)"
fi

ROOT_KB="$(du -sk "$ROOT" | awk '{print $1}')"
echo "Rootfs size: $((ROOT_KB / 1024)) MiB"

if [ "$IMAGE_MB" = "auto" ]; then
	ROOT_MB=$(((ROOT_KB + 1023) / 1024))
	IMAGE_MB=$((ROOT_MB + IMAGE_MARGIN_MB + 2))
	if [ "$IMAGE_MB" -lt "$IMAGE_MIN_MB" ]; then
		IMAGE_MB="$IMAGE_MIN_MB"
	fi
	IMAGE_MB=$((((IMAGE_MB + 3) / 4) * 4))
	echo "Auto image size: ${IMAGE_MB} MiB (min=${IMAGE_MIN_MB}, margin=${IMAGE_MARGIN_MB})"
fi

echo "Creating ${IMAGE_MB} MiB disk image ..."
mkdir -p "$IMAGE_DIR"
rm -f "$IMG"
truncate -s "${IMAGE_MB}M" "$IMG"

echo "label: dos
unit: sectors
start=${PART_START}, size=-, type=83, bootable" | sfdisk "$IMG"

IMAGE_BYTES=$((IMAGE_MB * 1024 * 1024))
IMAGE_SECTORS=$((IMAGE_BYTES / 512))
PART_SECTORS=$((IMAGE_SECTORS - PART_START))
PART_BYTES=$((PART_SECTORS * 512))
PART_KB=$((PART_BYTES / 1024))
PART_MIB=$((PART_KB / 1024))
# Stage partition + GRUB scratch files on native Linux fs (fast, avoids DrvFS quirks).
LINUX_WORK="${ALPINE_LINUX_WORK:-/tmp/leap-alpine-build-${USER:-user}}"
PART_IMG="$LINUX_WORK/partition.ext4"
mkdir -p "$LINUX_WORK"

if [ "$ROOT_KB" -gt $((PART_KB * 95 / 100)) ]; then
	echo "error: rootfs ($((ROOT_KB / 1024)) MiB) too large for ${PART_MIB} MiB partition" >&2
	echo "Largest paths:" >&2
	du -sh "$ROOT"/* 2>/dev/null | sort -h | tail -8 >&2
	exit 1
fi

# Last console= wins as primary /dev/console — keep ttyS0 last so OpenRC and
# boot messages land on COM1 (headless board, QEMU -nographic).
KERNEL_ARGS="root=LABEL=LEAPGW rootfstype=ext4 rootwait modules=sd-mod,ext4 earlyprintk=serial,ttyS0,115200 console=tty1 console=ttyS0,115200n8"

# Applied at pack time so the cached rootfs picks up fixes without FORCE_ROOTFS=1.
echo "Applying boot fixups ..."
# Alpine minirootfs ships root locked (root:!:) — unlock for console login.
sed -i 's/^root:[^:]*:/root::/' "$ROOT/etc/shadow"
# Re-apply overlay every pack so overlay edits don't require a rootfs rebuild.
rsync -a "$SCRIPT_DIR/overlay/" "$ROOT/"
# CRLF fixup for scripts/configs edited on Windows — grep -I skips ELF
# binaries (sed would strip \r bytes inside them and corrupt the ELF).
find "$ROOT/etc" "$ROOT/usr/sbin" -type f \
	-exec grep -Iq . {} \; -exec sed -i 's/\r$//' {} \; 2>/dev/null || true
chmod 755 "$ROOT/usr/sbin/leap-gateway" "$ROOT/etc/init.d/leap-gateway"
if [ -e "$ROOT/etc/init.d/leap-growfs" ]; then
	chmod 755 "$ROOT/etc/init.d/leap-growfs"
fi
# rc-update inside the chroot failed silently — enable services as plain
# symlinks (exactly what rc-update does). Minimal appliance set:
#   sysinit: device nodes + driver load
#   boot:    fsck, remount / rw, fstab mounts, hostname, logging
#   default: the gateway daemon
#   shutdown: clean unmount
enable_svc() {
	local runlevel="$1" svc="$2"
	if [ ! -e "$ROOT/etc/init.d/$svc" ]; then
		echo "warn: init script missing, skipping: $svc" >&2
		return 0
	fi
	mkdir -p "$ROOT/etc/runlevels/$runlevel"
	ln -snf "/etc/init.d/$svc" "$ROOT/etc/runlevels/$runlevel/$svc"
}
enable_svc sysinit devfs
enable_svc sysinit dmesg
enable_svc sysinit mdev
enable_svc sysinit hwdrivers
enable_svc boot fsck
enable_svc boot root
enable_svc boot localmount
enable_svc boot leap-growfs
enable_svc boot modules
enable_svc boot sysctl
enable_svc boot hostname
enable_svc boot bootmisc
enable_svc boot syslog
enable_svc default leap-gateway
# Stub gateway does its own IP bring-up from /cf/config.txt — Alpine's
# networking service has no /etc/network/interfaces and would just fail.
rm -f "$ROOT/etc/runlevels/default/networking"
enable_svc shutdown killprocs
enable_svc shutdown savecache
enable_svc shutdown mount-ro

echo "Writing GRUB config into rootfs ..."
mkdir -p "$ROOT/boot/grub"
cat > "$ROOT/boot/grub/grub.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set default=0
set timeout=1

menuentry "LeapOS-Gateway (Alpine i386, COM1 115200)" {
	linux /boot/vmlinuz-lts ${KERNEL_ARGS}
	initrd /boot/initramfs-lts
	boot
}
EOF
# Legacy extlinux dir is no longer used for boot; drop it if present.
rm -rf "$ROOT/boot/extlinux" "$ROOT/boot/ldlinux.sys"

FS_BLOCKS=$((PART_BYTES / 4096))
echo "Packing rootfs with mke2fs ext4 (${PART_MIB} MiB, staging on ${LINUX_WORK}, no loop mount) ..."
rm -f "$PART_IMG"
mke2fs -q -t ext4 -b 4096 -L LEAPGW -d "$ROOT" "$PART_IMG" "$FS_BLOCKS"

echo "Writing partition into disk image (seek sector ${PART_START}) ..."
dd if="$PART_IMG" of="$IMG" bs=512 seek="$PART_START" conv=notrunc status=none

echo "Embedding GRUB (boot.img + core.img, no loop mount) ..."
cat > "$LINUX_WORK/early.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set root=(hd0,msdos1)
echo "LeapOS-Gateway: loading Alpine kernel..."
linux /boot/vmlinuz-lts ${KERNEL_ARGS}
initrd /boot/initramfs-lts
boot
EOF

# shellcheck disable=SC2086
grub-mkimage -O i386-pc -o "$LINUX_WORK/core.img" -d "$GRUB_DIR" \
	--prefix='(hd0,msdos1)/boot/grub' \
	-c "$LINUX_WORK/early.cfg" \
	$GRUB_MODS

core_bytes="$(wc -c < "$LINUX_WORK/core.img" | tr -d ' ')"
if [ "$core_bytes" -gt $(((PART_START - 1) * 512)) ]; then
	echo "error: GRUB core.img too large for post-MBR gap (${core_bytes} bytes)" >&2
	exit 1
fi

dd if="$GRUB_DIR/boot.img" of="$IMG" conv=notrunc bs=446 count=1 status=none
dd if="$LINUX_WORK/core.img" of="$IMG" conv=notrunc bs=512 seek=1 status=none

ls -lh "$IMG"
echo ""
echo "Done: $IMG"
echo "Flash with Etcher or:"
echo "  dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
echo "Serial: COM1 115200 8N1"
