#!/bin/bash
# Build MBR + ext4 Alpine i386 disk image — LeapOS NetBoot Server for D945GSEJT.
#   bash mk-image.sh                        # repack from cached rootfs
#   sudo FORCE_ROOTFS=1 bash mk-image.sh    # full rootfs rebuild (first time)
set -euo pipefail

_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NETBOOT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LEAPOS_ROOT="$(cd "$SCRIPT_DIR/../../platforms/x86-32/D945GSEJT/LeapOS" && pwd)"
IMAGE_DIR="${LEAP_NETBOOT_IMAGE_DIR:-$LEAPOS_ROOT/rtems-image}"
IMG="${LEAP_NETBOOT_D945_IMG:-$IMAGE_DIR/leap-netboot-server-d945.img}"
CACHE="$SCRIPT_DIR/cache"
APK_CACHE="$CACHE/apk"

ALPINE_RELEASE="${ALPINE_RELEASE:-3.20.6}"
ALPINE_BRANCH="${ALPINE_BRANCH:-$(echo "$ALPINE_RELEASE" | cut -d. -f1,2)}"
IMAGE_MB="${IMAGE_MB:-auto}"
IMAGE_MIN_MB="${IMAGE_MIN_MB:-160}"
IMAGE_MARGIN_MB="${IMAGE_MARGIN_MB:-32}"
PART_START=2048
PART_ALIGN=2048
WORK="$SCRIPT_DIR/build-work"
ROOT="$WORK/rootfs"
MINIROOT="$CACHE/alpine-minirootfs-${ALPINE_RELEASE}-x86.tar.gz"
MINIROOT_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_BRANCH}/releases/x86/alpine-minirootfs-${ALPINE_RELEASE}-x86.tar.gz"
ROOTFS_STAMP="$ROOT/.leap-netboot-i386-rootfs-ready"
PKG_HASH_FILE="$ROOT/.leap-netboot-i386-packages-hash"
LEAP_DEVICE_PXE_BUNDLE="${LEAP_DEVICE_PXE_BUNDLE:-$IMAGE_DIR/leap-device-alpine-pxe.tar.gz}"
BUNDLED_DEVICE_ID="${BUNDLED_DEVICE_ID:-leapdevice001}"
BUNDLED_DEVICE_NAME="${BUNDLED_DEVICE_NAME:-LeapOS device Alpine (PXE)}"
BUNDLED_HTTP_BOOT_SERVER="${BUNDLED_HTTP_BOOT_SERVER:-172.16.82.188}"
BUNDLED_KERNEL_ARGS="ip=dhcp modules=loop,squashfs,sd-mod,ext4,r8169 earlyprintk=serial,ttyS0,115200 console=tty1 console=ttyS0,115200n8"
LEAP_TIMEZONE="${LEAP_TIMEZONE:-America/Chicago}"

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

image_part_sectors() {
	local img="$1" sectors=""

	sectors="$(sfdisk --list --bytes -o Start,Sectors "$img" 2>/dev/null |
		awk -v start="$PART_START" 'NR > 1 && $1 == start { print $2; exit }')"
	if [ -n "$sectors" ]; then
		echo "$sectors"
		return 0
	fi

	sfdisk -l "$img" 2>/dev/null |
		awk -v start="$PART_START" '$3 == start { print $5; exit }'
}

verify_disk_image() {
	local img="$1" expect_part_sectors="$2" expect_fs_blocks="$3"
	local img_bytes part_sectors fs_blocks part_img_bytes

	img_bytes="$(wc -c < "$img" | tr -d ' ')"
	if [ "$img_bytes" -ne "$IMAGE_BYTES" ]; then
		echo "error: $img is ${img_bytes} bytes, expected ${IMAGE_BYTES}" >&2
		exit 1
	fi

	part_sectors="$(image_part_sectors "$img")"
	if [ -z "$part_sectors" ] || [ "$part_sectors" -ne "$expect_part_sectors" ]; then
		echo "error: partition size mismatch in $img (got ${part_sectors:-?}, want ${expect_part_sectors})" >&2
		sfdisk -l "$img" >&2 || true
		exit 1
	fi

	part_img_bytes="$(wc -c < "$PART_IMG" | tr -d ' ')"
	if [ "$part_img_bytes" -gt "$PART_BYTES" ]; then
		echo "error: ext4 blob (${part_img_bytes} bytes) exceeds partition (${PART_BYTES} bytes)" >&2
		exit 1
	fi

	fs_blocks="$(tune2fs -l "$PART_IMG" 2>/dev/null | awk '/^Block count:/{print $3; exit}')"
	if [ -z "$fs_blocks" ] || [ "$fs_blocks" -gt "$expect_fs_blocks" ]; then
		echo "error: ext4 block count (${fs_blocks:-?}) exceeds partition allowance (${expect_fs_blocks})" >&2
		exit 1
	fi
}

GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"
GRUB_MODS="biosdisk part_msdos ext2 linux serial terminal echo gzio"
if [ ! -f "$GRUB_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc files not found at $GRUB_DIR" >&2
	exit 1
fi

QEMU="$(command -v qemu-i386-static || true)"
if [ -z "$QEMU" ]; then
	echo "error: qemu-i386-static not found (needed for i386 chroot on x86_64 host)" >&2
	exit 1
fi

need_cmd wget tar sfdisk rsync chroot mke2fs tune2fs grub-mkimage dd wc

grub_pick_mods() {
	local mod
	for mod in "$@"; do
		if [ -f "$GRUB_DIR/${mod}.mod" ]; then
			echo -n "$mod "
		fi
	done
}

resolve_boot_images() {
	local vmlinuz initrd

	vmlinuz="$(ls "$ROOT"/boot/vmlinuz-* 2>/dev/null | sort | head -1)"
	initrd="$(ls "$ROOT"/boot/initramfs-* 2>/dev/null | sort | head -1)"

	if [ -z "$vmlinuz" ] || [ ! -f "$vmlinuz" ]; then
		echo "error: no vmlinuz in $ROOT/boot (is linux-lts installed?)" >&2
		exit 1
	fi
	if [ -z "$initrd" ] || [ ! -f "$initrd" ]; then
		echo "error: no initramfs in $ROOT/boot" >&2
		exit 1
	fi

	VMLINUZ="/boot/$(basename "$vmlinuz")"
	INITRD="/boot/$(basename "$initrd")"
	echo "Boot: ${VMLINUZ} + ${INITRD}"
}

install_app_tree() {
	echo "Installing NetBoot application tree into rootfs ..."
	mkdir -p "$ROOT/opt/leap-netboot"
	rsync -a --delete \
		--exclude '__pycache__' \
		--exclude '*.pyc' \
		"$NETBOOT_ROOT/server/" "$ROOT/opt/leap-netboot/server/"
	rsync -a --delete \
		"$NETBOOT_ROOT/web/" "$ROOT/opt/leap-netboot/web/"
	rsync -a \
		"$NETBOOT_ROOT/scripts/" "$ROOT/opt/leap-netboot/scripts/"
	chmod 755 "$ROOT/opt/leap-netboot/scripts/"*.sh 2>/dev/null || true
	chmod 755 "$ROOT/opt/leap-netboot/server/leap_netbootd.py"
}

seed_bundled_device_image() {
	if [ "${SKIP_DEVICE_SEED:-0}" = "1" ]; then
		echo "Skipping bundled device PXE seed (SKIP_DEVICE_SEED=1)"
		return 0
	fi

	local bundle="$LEAP_DEVICE_PXE_BUNDLE"
	local state="$ROOT/var/lib/leap-netboot/state"
	local imgroot="$ROOT/var/lib/leap-netboot/tftp/images"
	local dest="$imgroot/$BUNDLED_DEVICE_ID"
	local vmlinuz initramfs modloop apkovl size_bytes created

	if [ ! -f "$bundle" ]; then
		echo "error: bundled device PXE tar missing: $bundle" >&2
		echo "Build it first: NetbootServer/alpine-i386/build-d945-lab.sh" >&2
		exit 1
	fi

	echo "Seeding bundled LeapOS device PXE image from $bundle ..."
	mkdir -p "$state" "$dest"
	rm -rf "$dest"
	mkdir -p "$dest"
	tar -xzf "$bundle" -C "$dest"

	vmlinuz="vmlinuz-lts"
	initramfs="initramfs-lts"
	modloop="modloop-lts"
	apkovl="leap-device.apkovl.tar.gz"
	for f in "$vmlinuz" "$initramfs" "$modloop" "$apkovl"; do
		if [ ! -f "$dest/$f" ]; then
			echo "error: bundle missing $f (have: $(ls -1 "$dest" | tr '\n' ' '))" >&2
			exit 1
		fi
	done
	for apkovl_entry in \
		'etc/.default_boot_services' \
		'etc/runlevels/sysinit/modloop' \
		'etc/runlevels/boot/networking' \
		'etc/runlevels/default/local' \
		'etc/local.d/leap-device.start'; do
		if ! tar -tzf "$dest/$apkovl" | sed 's#^\./##' | grep -qx "$apkovl_entry"; then
			echo "error: bundled apkovl missing $apkovl_entry" >&2
			exit 1
		fi
	done

	size_bytes=$(( \
		$(wc -c < "$dest/$vmlinuz" | tr -d ' ') + \
		$(wc -c < "$dest/$initramfs" | tr -d ' ') + \
		$(wc -c < "$dest/$modloop" | tr -d ' ') + \
		$(wc -c < "$dest/$apkovl" | tr -d ' ') \
	))
	created="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

	python3 <<PY
import json
from pathlib import Path

state = Path("$state")
meta = {
    "id": "$BUNDLED_DEVICE_ID",
    "name": """$BUNDLED_DEVICE_NAME""",
    "type": "alpine-linux",
    "filename": "$modloop",
    "vmlinuz": "$vmlinuz",
    "initramfs": "$initramfs",
    "modloop": "$modloop",
    "apkovl": "$apkovl",
    "alpine_repo": "http://dl-cdn.alpinelinux.org/alpine/v3.20/main",
    "kernel_args": """$BUNDLED_KERNEL_ARGS""",
    "size_bytes": int("$size_bytes"),
    "created": "$created",
}
(state / "images.json").write_text(
    json.dumps({"$BUNDLED_DEVICE_ID": meta}, indent=2) + "\n"
)
settings = {
    "default_image_id": "$BUNDLED_DEVICE_ID",
    "server_label": "LeapOS NetBoot",
    "http_boot_base": "/httpboot",
    "http_boot_server": "$BUNDLED_HTTP_BOOT_SERVER",
}
(state / "settings.json").write_text(json.dumps(settings, indent=2) + "\n")
(state / "devices.json").write_text("{}\n")
PY

	echo "Bundled device image: $BUNDLED_DEVICE_ID ($(du -sh "$dest" | awk '{print $1}'))"
}

build_pxe_grub() {
	local tftp="$ROOT/var/lib/leap-netboot/tftp"
	local grub_dir="$tftp/boot/grub/i386-pc"
	local pxe_mods

	echo "Building GRUB i386-pc-pxe loader into TFTP tree ..."
	mkdir -p "$grub_dir"
	rm -f "$grub_dir"/*.mod "$grub_dir/core.0"
	pxe_mods="$(grub_pick_mods \
		pxe tftp net linux multiboot serial terminal gzio normal configfile echo \
		e1000 ne2k_pci rtl8139 rtl8168 ata pata)"
	if [ -z "$pxe_mods" ]; then
		echo "error: no GRUB PXE modules found under $GRUB_DIR" >&2
		exit 1
	fi
	# shellcheck disable=SC2086
	grub-mkimage \
		-O i386-pc-pxe \
		-o "$grub_dir/core.0" \
		-d "$GRUB_DIR" \
		-p /boot/grub \
		$pxe_mods
	for mod in $pxe_mods; do
		if [ -f "$GRUB_DIR/${mod}.mod" ]; then
			cp "$GRUB_DIR/${mod}.mod" "$grub_dir/"
		fi
	done
}

trim_rootfs() {
	echo "Trimming rootfs (keep r8169 + storage drivers, drop bulk firmware/docs) ..."
	chroot "$ROOT" /bin/sh <<'TRIM'
set -e
find /lib/firmware -mindepth 1 -maxdepth 1 ! -name rtl_nic -exec rm -rf {} + 2>/dev/null || true
rm -rf /usr/share/man /usr/share/doc /var/cache/apk/*
rm -f /boot/modloop-* 2>/dev/null || true
KVER="$(ls -1 /lib/modules 2>/dev/null | grep -v -E 'microsoft|WSL' | head -1)"
if [ -z "$KVER" ] || [ ! -d "/lib/modules/$KVER" ]; then
	echo "error: Alpine kernel modules not found under /lib/modules" >&2
	exit 1
fi
keep_ko() {
	case "$1" in
	*/kernel/drivers/ata/*|*/kernel/drivers/scsi/*|*/kernel/drivers/block/loop.ko*|\
	*/kernel/drivers/net/r8169.ko*|*/kernel/drivers/net/r8169.ko.xz|\
	*/kernel/drivers/net/ethernet/realtek/*|*/kernel/drivers/net/phy/*|\
	*/kernel/drivers/net/mdio/*|*/kernel/fs/ext4/*|*/kernel/lib/crc/*) return 0 ;;
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
	fi

	unmount_chroot_mounts "$ROOT"
	rm -rf "$ROOT"
	mkdir -p "$ROOT"
	tar -xzf "$MINIROOT" -C "$ROOT"
	install -m 755 "$QEMU" "$ROOT/usr/bin/qemu-i386-static"

	_pkg_list="$WORK/apk-packages.lst"
	sed 's/\r$//' "$SCRIPT_DIR/packages.txt" |
		grep -v '^#' |
		grep -v '^[[:space:]]*$' > "$_pkg_list"
	mapfile -t _apk_pkgs < "$_pkg_list"

	cp /etc/resolv.conf "$ROOT/etc/resolv.conf"
	mkdir -p "$ROOT/var/cache/apk"
	mount -t proc proc "$ROOT/proc"
	mount -t sysfs sys "$ROOT/sys"
	mount --bind /dev "$ROOT/dev"
	mount --bind "$APK_CACHE" "$ROOT/var/cache/apk"

	chroot "$ROOT" /sbin/apk update
	chroot "$ROOT" /sbin/apk add "${_apk_pkgs[@]}"

	apply_overlay
	trim_rootfs
	md5sum "$SCRIPT_DIR/packages.txt" | awk '{print $1}' > "$PKG_HASH_FILE"
	touch "$ROOTFS_STAMP"

	unmount_chroot_mounts "$ROOT"
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

apply_overlay() {
	rsync -a "$SCRIPT_DIR/overlay/" "$ROOT/"
	install_app_tree
	printf '%s\n' "$LEAP_TIMEZONE" > "$ROOT/etc/timezone"
	if [ -f "$ROOT/usr/share/zoneinfo/$LEAP_TIMEZONE" ]; then
		cp "$ROOT/usr/share/zoneinfo/$LEAP_TIMEZONE" "$ROOT/etc/localtime"
	else
		echo "warn: timezone data missing for $LEAP_TIMEZONE" >&2
	fi
	mkdir -p "$ROOT/etc/nginx/http.d"
	rsync -a --delete "$SCRIPT_DIR/overlay/etc/nginx/http.d/" "$ROOT/etc/nginx/http.d/"
	rm -f "$ROOT/etc/nginx/http.d/default.conf"
	rm -f "$ROOT/etc/nginx/http.d/00-body-size.conf"
	rm -f "$ROOT/etc/local.d/nginx-upload-limits.start"
	rm -f "$ROOT/etc/conf.d/in.tftpd"
	find "$ROOT/etc" "$ROOT/opt/leap-netboot" -type f \
		\( -name '*.sh' -o -name '*.py' -o -name '*.conf' -o -path '*/init.d/*' -o -path '*/conf.d/*' -o -name 'inittab' -o -name 'modules' -o -name 'fstab' \) \
		-exec sed -i 's/\r$//' {} \; 2>/dev/null || true
	chmod 755 "$ROOT/etc/local.d/nic-modprobe.start" 2>/dev/null || true
	for svc in leap-growfs leap-netboot-dirs leap-netboot in.tftpd; do
		if [ -e "$ROOT/etc/init.d/$svc" ]; then
			chmod 755 "$ROOT/etc/init.d/$svc"
		fi
	done
}

verify_packaged_rootfs() {
	local fail=0

	check() {
		if [ ! -e "$ROOT/$1" ]; then
			echo "error: packaged rootfs missing $1" >&2
			fail=1
		fi
	}

	check etc/init.d/in.tftpd
	check etc/init.d/leap-netboot-dirs
	check etc/init.d/leap-netboot
	check etc/nginx/http.d/leap-netboot.conf
	check etc/conf.d/ntpd
	check etc/localtime
	check etc/timezone
	check opt/leap-netboot/server/leap_netbootd.py
	check opt/leap-netboot/web/index.html

	if [ -f "$ROOT/etc/nginx/http.d/00-body-size.conf" ]; then
		echo "error: stale etc/nginx/http.d/00-body-size.conf must not be in image" >&2
		fail=1
	fi
	if [ -f "$ROOT/etc/conf.d/in.tftpd" ]; then
		echo "error: stale etc/conf.d/in.tftpd must not be in image (use init.d/in.tftpd)" >&2
		fail=1
	fi
	if grep -q 'size=64m' "$ROOT/etc/fstab" 2>/dev/null; then
		echo "error: etc/fstab still caps /tmp at 64m" >&2
		fail=1
	fi
	if ! grep -q 'client_max_body_size 512m' "$ROOT/etc/nginx/http.d/leap-netboot.conf" 2>/dev/null; then
		echo "error: nginx upload limit not configured" >&2
		fail=1
	fi
	if ! grep -q 'client_body_temp_path' "$ROOT/etc/nginx/http.d/leap-netboot.conf" 2>/dev/null; then
		echo "error: nginx client_body_temp_path not on root disk" >&2
		fail=1
	fi
	if ! grep -q '_handle_alpine_upload' "$ROOT/opt/leap-netboot/server/leap_netbootd.py" 2>/dev/null; then
		echo "error: leap_netbootd missing Alpine upload handler" >&2
		fail=1
	fi
	if ! grep -q 'apkovl' "$ROOT/opt/leap-netboot/scripts/render-pxe.sh" 2>/dev/null; then
		echo "error: render-pxe.sh missing apkovl support" >&2
		fail=1
	fi
	if ! grep -q 'apkovl' "$ROOT/opt/leap-netboot/server/leap_netbootd.py" 2>/dev/null; then
		echo "error: leap_netbootd missing apkovl support" >&2
		fail=1
	fi
	if ! grep -q 'booted-devices' "$ROOT/opt/leap-netboot/server/leap_netbootd.py" 2>/dev/null; then
		echo "error: leap_netbootd missing booted device report API" >&2
		fail=1
	fi
	if ! grep -q '^America/Chicago$' "$ROOT/etc/timezone" 2>/dev/null; then
		echo "error: timezone is not America/Chicago" >&2
		fail=1
	fi
	if [ ! -L "$ROOT/etc/runlevels/default/ntpd" ] && [ ! -e "$ROOT/etc/runlevels/default/ntpd" ]; then
		echo "error: ntpd not enabled in default runlevel" >&2
		fail=1
	fi
	if [ "${SKIP_DEVICE_SEED:-0}" != "1" ]; then
		local devdir="$ROOT/var/lib/leap-netboot/tftp/images/$BUNDLED_DEVICE_ID"
		for f in vmlinuz-lts initramfs-lts modloop-lts leap-device.apkovl.tar.gz; do
			if [ ! -f "$devdir/$f" ]; then
				echo "error: seeded device image missing $devdir/$f" >&2
				fail=1
			fi
		done
		if ! grep -q "$BUNDLED_DEVICE_ID" "$ROOT/var/lib/leap-netboot/state/images.json" 2>/dev/null; then
			echo "error: images.json missing bundled device $BUNDLED_DEVICE_ID" >&2
			fail=1
		fi
		if ! grep -q '"apkovl"' "$ROOT/var/lib/leap-netboot/state/images.json" 2>/dev/null; then
			echo "error: images.json missing apkovl metadata" >&2
			fail=1
		fi
	fi

	if [ "$fail" -ne 0 ]; then
		exit 1
	fi
	echo "Packaged rootfs checks passed"
}

unmount_chroot_mounts "$ROOT"
trap 'unmount_chroot_mounts "$ROOT"' EXIT

echo "=== LeapOS NetBoot Server — Alpine i386 (D945GSEJT) ==="
echo "Release: Alpine ${ALPINE_RELEASE} x86 (i386)"
echo "Output:  $IMG"

if need_rootfs_rebuild; then
	if [ "$(id -u)" -ne 0 ]; then
		echo "error: rootfs build needs root: sudo FORCE_ROOTFS=1 bash $0" >&2
		exit 1
	fi
	build_rootfs
else
	echo "Reusing cached rootfs ($ROOT)"
fi

echo "Applying overlay + app tree (pack-time refresh) ..."
apply_overlay

sed -i 's/^root:[^:]*:/root::/' "$ROOT/etc/shadow"

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
enable_svc boot networking
enable_svc default leap-netboot-dirs
enable_svc default in.tftpd
enable_svc default leap-netboot
enable_svc default nginx
enable_svc default ntpd
enable_svc default crond
enable_svc shutdown killprocs
enable_svc shutdown savecache
enable_svc shutdown mount-ro

seed_bundled_device_image
build_pxe_grub
if [ -x "$NETBOOT_ROOT/scripts/render-pxe.sh" ]; then
	LEAP_NETBOOT_ROOT="$ROOT/var/lib/leap-netboot" \
		"$NETBOOT_ROOT/scripts/render-pxe.sh" || true
fi

KERNEL_ARGS="root=LABEL=LEAPNB rootfstype=ext4 rootwait modules=sd-mod,ext4 loglevel=3 console=ttyS0,115200n8"
resolve_boot_images

mkdir -p "$ROOT/boot/grub"
cat > "$ROOT/boot/grub/grub.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set default=0
set timeout=2

menuentry "LeapOS NetBoot Server (Alpine i386)" {
	linux ${VMLINUZ} ${KERNEL_ARGS}
	initrd ${INITRD}
	boot
}
EOF
rm -rf "$ROOT/boot/extlinux" "$ROOT/boot/ldlinux.sys"

verify_packaged_rootfs

ROOTFS_KB="$(du -sk "$ROOT" | awk '{print $1}')"
echo "Packed rootfs size: $((ROOTFS_KB / 1024)) MiB"

if [ "$IMAGE_MB" = "auto" ]; then
	ROOT_MB=$(((ROOTFS_KB + 1023) / 1024))
	# ext4 metadata + small headroom; CF grows to full size on first boot (leap-growfs).
	IMAGE_MB=$((ROOT_MB + IMAGE_MARGIN_MB + (ROOT_MB / 10) + 4))
	if [ "$IMAGE_MB" -lt "$IMAGE_MIN_MB" ]; then
		IMAGE_MB="$IMAGE_MIN_MB"
	fi
	IMAGE_MB=$((((IMAGE_MB + 3) / 4) * 4))
	echo "Auto image size: ${IMAGE_MB} MiB (packed root ${ROOT_MB} MiB + margin)"
fi

IMAGE_BYTES=$((IMAGE_MB * 1024 * 1024))
IMAGE_SECTORS=$((IMAGE_BYTES / 512))
PART_SECTORS=$(( (IMAGE_SECTORS - PART_START) / PART_ALIGN * PART_ALIGN ))
PART_BYTES=$((PART_SECTORS * 512))
PART_KB=$((PART_BYTES / 1024))
PART_MIB=$((PART_KB / 1024))
FS_BLOCKS=$((PART_BYTES / 4096))
LINUX_WORK="${ALPINE_LINUX_WORK:-/tmp/leap-netboot-i386-build-${USER:-user}}"
PART_IMG="$LINUX_WORK/partition.ext4"
mkdir -p "$LINUX_WORK" "$IMAGE_DIR"

if [ "$ROOTFS_KB" -gt $((PART_KB * 88 / 100)) ]; then
	echo "error: packed rootfs ($((ROOTFS_KB / 1024)) MiB) too large for ${PART_MIB} MiB partition" >&2
	echo "Largest paths:" >&2
	du -sh "$ROOT"/* 2>/dev/null | sort -h | tail -8 >&2
	echo "Set IMAGE_MB= larger or FORCE_ROOTFS=1 to trim rootfs." >&2
	exit 1
fi

rm -f "$IMG"
echo "Creating ${IMAGE_MB} MiB disk image ..."
truncate -s "$IMAGE_BYTES" "$IMG"

printf '%s\n' \
	'label: dos' \
	'unit: sectors' \
	"start=${PART_START}, size=${PART_SECTORS}, type=83, bootable" |
	sfdisk --force "$IMG" >/dev/null

rm -f "$PART_IMG"
echo "Packing rootfs into ext4 (${PART_MIB} MiB) ..."
mke2fs -q -t ext4 -b 4096 -L LEAPNB -d "$ROOT" "$PART_IMG" "$FS_BLOCKS"

dd if="$PART_IMG" of="$IMG" bs=512 seek="$PART_START" count="$PART_SECTORS" \
	conv=notrunc status=none
verify_disk_image "$IMG" "$PART_SECTORS" "$FS_BLOCKS"

cat > "$LINUX_WORK/early.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set root=(hd0,msdos1)
linux ${VMLINUZ} ${KERNEL_ARGS}
initrd ${INITRD}
boot
EOF

# shellcheck disable=SC2086
grub-mkimage -O i386-pc -o "$LINUX_WORK/core.img" -d "$GRUB_DIR" \
	--prefix='(hd0,msdos1)/boot/grub' \
	-c "$LINUX_WORK/early.cfg" \
	$GRUB_MODS

core_bytes="$(wc -c < "$LINUX_WORK/core.img" | tr -d ' ')"
if [ "$core_bytes" -gt $(( (PART_START - 1) * 512 )) ]; then
	echo "error: GRUB core.img too large for post-MBR gap (${core_bytes} bytes)" >&2
	exit 1
fi

dd if="$GRUB_DIR/boot.img" of="$IMG" conv=notrunc bs=446 count=1 status=none
dd if="$LINUX_WORK/core.img" of="$IMG" conv=notrunc bs=512 seek=1 status=none
verify_disk_image "$IMG" "$PART_SECTORS" "$FS_BLOCKS"

ls -lh "$IMG"
echo ""
echo "Done: $IMG"
echo "Target: Intel D945GSEJT (Atom N270, i386, legacy BIOS, COM1 115200)"
echo "Flash: dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
echo "Web UI: http://<server-ip>/  (after DHCP on eth0)"
echo "PXE: bundled device image $BUNDLED_DEVICE_ID pre-loaded — no upload required"
echo "First boot auto-detects server IP and regenerates client GRUB menu (apkovl + modloop)"
