#!/bin/bash
# Build GPT hybrid (UEFI + legacy BIOS) Alpine x86_64 disk image for LeapOS NetBoot.
#   bash mk-image.sh              # repack from cached rootfs
#   sudo FORCE_ROOTFS=1 bash mk-image.sh   # full rootfs rebuild
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NETBOOT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_DIR="${LEAP_NETBOOT_IMAGE_DIR:-$SCRIPT_DIR/build-work}"
IMG="${LEAP_NETBOOT_IMG:-$IMAGE_DIR/leap-netboot-server.img}"
CACHE="$SCRIPT_DIR/cache"
APK_CACHE="$CACHE/apk"

ALPINE_RELEASE="${ALPINE_RELEASE:-3.20.6}"
ALPINE_BRANCH="${ALPINE_BRANCH:-$(echo "$ALPINE_RELEASE" | cut -d. -f1,2)}"
IMAGE_MB="${IMAGE_MB:-2048}"
IMAGE_MIN_MB="${IMAGE_MIN_MB:-1280}"
IMAGE_MARGIN_MB="${IMAGE_MARGIN_MB:-256}"
PART_ALIGN=2048
# GPT: 1 MiB BIOS boot + ESP + ext4 root (hybrid — UEFI and legacy BIOS).
BIOS_BOOT_START=2048
BIOS_BOOT_SECTORS=2048
ESP_MB="${ESP_MB:-256}"
ESP_SECTORS=$((ESP_MB * 1024 * 1024 / 512))
# Backup GPT header at end of disk (sfdisk 2.x).
GPT_END_RESERVE=34
# sfdisk 2.39 rejects short type codes (EF02/EF00/8300) — use full GUIDs.
PART_TYPE_BIOS="21686148-6449-6E6F-744E-656564454649"
PART_TYPE_ESP="C12A7328-F81F-11D2-BA4B-00A0C93EC93B"
PART_TYPE_LINUX="0FC63DAF-8483-4772-8E69-6D6964693630"
WORK="$SCRIPT_DIR/build-work"
ROOT="$WORK/rootfs"
MINIROOT="$CACHE/alpine-minirootfs-${ALPINE_RELEASE}-x86_64.tar.gz"
MINIROOT_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_BRANCH}/releases/x86_64/alpine-minirootfs-${ALPINE_RELEASE}-x86_64.tar.gz"
ROOTFS_STAMP="$ROOT/.leap-netboot-rootfs-ready"
PKG_HASH_FILE="$ROOT/.leap-netboot-packages-hash"

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
		exit 1
	fi
}

image_part_sectors() {
	local img="$1" start="$2" sectors=""

	sectors="$(sfdisk --list --bytes -o Start,Sectors "$img" 2>/dev/null |
		awk -v start="$start" 'NR > 1 && $1 == start { print $2; exit }')"
	if [ -n "$sectors" ]; then
		echo "$sectors"
		return 0
	fi

	sfdisk -l "$img" 2>/dev/null |
		awk -v start="$start" '$3 == start { print $5; exit }'
}

verify_disk_image() {
	local img="$1" root_start="$2" expect_root_sectors="$3" expect_fs_blocks="$4"
	local img_bytes part_sectors fs_blocks part_img_bytes

	img_bytes="$(wc -c < "$img" | tr -d ' ')"
	if [ "$img_bytes" -ne "$IMAGE_BYTES" ]; then
		echo "error: $img is ${img_bytes} bytes, expected ${IMAGE_BYTES}" >&2
		exit 1
	fi

	part_sectors="$(image_part_sectors "$img" "$root_start")"
	if [ -z "$part_sectors" ] || [ "$part_sectors" -ne "$expect_root_sectors" ]; then
		echo "error: root partition size mismatch in $img (got ${part_sectors:-?}, want ${expect_root_sectors})" >&2
		sfdisk -l "$img" >&2 || true
		exit 1
	fi

	part_img_bytes="$(wc -c < "$PART_IMG" | tr -d ' ')"
	if [ "$part_img_bytes" -gt "$ROOT_BYTES" ]; then
		echo "error: ext4 blob (${part_img_bytes} bytes) exceeds root partition (${ROOT_BYTES} bytes)" >&2
		exit 1
	fi

	fs_blocks="$(tune2fs -l "$PART_IMG" 2>/dev/null | awk '/^Block count:/{print $3; exit}')"
	if [ -z "$fs_blocks" ] || [ "$fs_blocks" -gt "$expect_fs_blocks" ]; then
		echo "error: ext4 block count (${fs_blocks:-?}) exceeds partition allowance (${expect_fs_blocks})" >&2
		exit 1
	fi
}

GRUB_BIOS_DIR="${GRUB_BIOS_DIR:-${GRUB_HOST_DIR:-/usr/lib/grub/i386-pc}}"
GRUB_EFI_DIR="${GRUB_EFI_DIR:-/usr/lib/grub/x86_64-efi}"
GRUB_BIOS_MODS="biosdisk part_gpt part_msdos ext2 linux serial terminal echo gzio normal configfile search search_label"
if [ ! -f "$GRUB_BIOS_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc files not found at $GRUB_BIOS_DIR" >&2
	echo "Install on build host: sudo apt install -y grub-pc-bin" >&2
	exit 1
fi
if [ ! -d "$GRUB_EFI_DIR" ] || [ ! -f "$GRUB_EFI_DIR/linux.mod" ]; then
	echo "error: GRUB x86_64-efi files not found at $GRUB_EFI_DIR" >&2
	echo "Install on build host: sudo apt install -y grub-efi-amd64-bin dosfstools mtools" >&2
	exit 1
fi

need_cmd wget tar sfdisk rsync chroot mke2fs tune2fs grub-mkimage dd wc mkfs.vfat mmd mcopy uuidgen

# Only pass .mod files that exist under the given GRUB module directory.
grub_pick_mods_from() {
	local dir="$1"; shift
	local mod
	for mod in "$@"; do
		if [ -f "$dir/${mod}.mod" ]; then
			echo -n "$mod "
		fi
	done
}

grub_pick_mods() {
	grub_pick_mods_from "$GRUB_BIOS_DIR" "$@"
}

resolve_boot_images() {
	local vmlinuz initrd

	vmlinuz="$(ls "$ROOT"/boot/vmlinuz-* 2>/dev/null | sort | head -1)"
	initrd="$(ls "$ROOT"/boot/initramfs-* 2>/dev/null | sort | head -1)"

	if [ -z "$vmlinuz" ] || [ ! -f "$vmlinuz" ]; then
		echo "error: no vmlinuz in $ROOT/boot (is linux-virt installed?)" >&2
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

build_pxe_grub() {
	local tftp="$ROOT/var/lib/leap-netboot/tftp"
	local grub_dir="$tftp/boot/grub/i386-pc"
	local pxe_mods

	echo "Building GRUB i386-pc-pxe loader into TFTP tree ..."
	mkdir -p "$grub_dir"
	# Legacy BIOS PXE — pxe/tftp/net, not efinet (EFI-only, absent on i386-pc).
	pxe_mods="$(grub_pick_mods \
		pxe tftp net multiboot serial terminal gzio normal configfile echo \
		e1000 ne2k_pci rtl8139 rtl8168 ata pata)"
	if [ -z "$pxe_mods" ]; then
		echo "error: no GRUB PXE modules found under $GRUB_BIOS_DIR" >&2
		exit 1
	fi
	# shellcheck disable=SC2086
	grub-mkimage \
		-O i386-pc-pxe \
		-o "$grub_dir/core.0" \
		-d "$GRUB_BIOS_DIR" \
		-p /boot/grub \
		$pxe_mods
	cp "$GRUB_BIOS_DIR"/*.mod "$grub_dir/" 2>/dev/null || true
}

build_esp_grub() {
	local esp_img="$LINUX_WORK/esp.fat"
	local esp_cfg="$LINUX_WORK/esp-grub.cfg"
	local esp_menu="$LINUX_WORK/esp-grub-menu.cfg"
	local efi_mods bootx64
	local vmlinuz_src initrd_src vmlinuz_name initrd_name

	vmlinuz_src="$(ls "$ROOT"/boot/vmlinuz-* 2>/dev/null | sort | head -1)"
	initrd_src="$(ls "$ROOT"/boot/initramfs-* 2>/dev/null | sort | head -1)"
	vmlinuz_name="$(basename "$vmlinuz_src")"
	initrd_name="$(basename "$initrd_src")"
	if [ -z "$vmlinuz_src" ] || [ ! -f "$vmlinuz_src" ]; then
		echo "error: no vmlinuz under $ROOT/boot for ESP staging" >&2
		exit 1
	fi
	if [ -z "$initrd_src" ] || [ ! -f "$initrd_src" ]; then
		echo "error: no initramfs under $ROOT/boot for ESP staging" >&2
		exit 1
	fi

	echo "Building UEFI ESP (${ESP_MB} MiB, kernel+initrd on FAT for headless UEFI) ..."

	cat > "$esp_menu" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set default=0
set timeout=3

menuentry "LeapOS NetBoot Server (Alpine)" {
	linux /boot/${vmlinuz_name} ${KERNEL_ARGS}
	initrd /boot/${initrd_name}
	boot
}
EOF

	cat > "$esp_cfg" <<'EOF'
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
configfile /EFI/BOOT/grub.cfg
EOF

	efi_mods="$(grub_pick_mods_from "$GRUB_EFI_DIR" \
		fat linux initrd gzio normal configfile serial terminal echo)"
	if [ -z "$efi_mods" ]; then
		echo "error: no GRUB EFI modules found under $GRUB_EFI_DIR" >&2
		exit 1
	fi

	bootx64="$LINUX_WORK/BOOTX64.EFI"
	# shellcheck disable=SC2086
	grub-mkimage -O x86_64-efi -o "$bootx64" -d "$GRUB_EFI_DIR" \
		-p /EFI/BOOT -c "$esp_cfg" \
		$efi_mods

	rm -f "$esp_img"
	truncate -s "$((ESP_SECTORS * 512))" "$esp_img"
	mkfs.vfat -F 32 -n LEAPESP "$esp_img"

	mmd -i "$esp_img" ::/EFI ::/EFI/BOOT ::/boot
	mcopy -i "$esp_img" "$bootx64" ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i "$esp_img" "$esp_menu" ::/EFI/BOOT/grub.cfg
	mcopy -i "$esp_img" "$vmlinuz_src" "::/boot/${vmlinuz_name}"
	mcopy -i "$esp_img" "$initrd_src" "::/boot/${initrd_name}"

	dd if="$esp_img" of="$IMG" bs=512 seek="$ESP_START" count="$ESP_SECTORS" \
		conv=notrunc status=none
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

	echo "Installing packages in x86_64 chroot (apk cache: $APK_CACHE) ..."
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
	install_app_tree
	find "$ROOT/etc" "$ROOT/opt/leap-netboot" -type f \
		\( -name '*.sh' -o -name '*.py' -o -name '*.conf' -o -path '*/init.d/*' \) \
		-exec sed -i 's/\r$//' {} \; 2>/dev/null || true

	chroot "$ROOT" /bin/sh -c 'rm -rf /var/cache/apk/* /usr/share/doc/* /usr/share/man/* 2>/dev/null || true'

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

unmount_chroot_mounts "$ROOT"
trap 'unmount_chroot_mounts "$ROOT"' EXIT

echo "=== LeapOS NetBoot Server — Alpine image build ==="
echo "Release: Alpine ${ALPINE_RELEASE} x86_64"
echo "Output:  $IMG"

if need_rootfs_rebuild; then
	if [ "$(id -u)" -ne 0 ]; then
		echo "error: rootfs build needs root: sudo FORCE_ROOTFS=1 bash $0" >&2
		exit 1
	fi
	build_rootfs
else
	echo "Reusing cached rootfs ($ROOT)"
	echo "  (set FORCE_ROOTFS=1 to re-run apk/chroot from scratch)"
fi

ROOTFS_KB="$(du -sk "$ROOT" | awk '{print $1}')"
echo "Rootfs size: $((ROOTFS_KB / 1024)) MiB"

if [ "$IMAGE_MB" = "auto" ]; then
	ROOT_MB=$(((ROOTFS_KB + 1023) / 1024))
	IMAGE_MB=$((ROOT_MB + ESP_MB + 2 + IMAGE_MARGIN_MB))
	if [ "$IMAGE_MB" -lt "$IMAGE_MIN_MB" ]; then
		IMAGE_MB="$IMAGE_MIN_MB"
	fi
	IMAGE_MB=$((((IMAGE_MB + 3) / 4) * 4))
	echo "Auto image size: ${IMAGE_MB} MiB (includes ${ESP_MB} MiB UEFI boot partition)"
fi

IMAGE_BYTES=$((IMAGE_MB * 1024 * 1024))
IMAGE_SECTORS=$((IMAGE_BYTES / 512))
ESP_START=$((BIOS_BOOT_START + BIOS_BOOT_SECTORS))
ROOT_START=$((ESP_START + ESP_SECTORS))
_avail_root=$((IMAGE_SECTORS - ROOT_START - GPT_END_RESERVE))
ROOT_SECTORS=$(( _avail_root - (_avail_root % PART_ALIGN) ))
ROOT_BYTES=$((ROOT_SECTORS * 512))
ROOT_PART_KB=$((ROOT_BYTES / 1024))
ROOT_MIB=$((ROOT_PART_KB / 1024))
FS_BLOCKS=$((ROOT_BYTES / 4096))
LINUX_WORK="${ALPINE_LINUX_WORK:-/tmp/leap-netboot-build-${USER:-user}}"
PART_IMG="$LINUX_WORK/partition.ext4"
mkdir -p "$LINUX_WORK" "$IMAGE_DIR"

if [ "$ROOT_SECTORS" -le 0 ]; then
	echo "error: disk too small (IMAGE_MB=${IMAGE_MB}; need room for UEFI boot + ext4 root)" >&2
	exit 1
fi

if [ "$ROOTFS_KB" -gt $((ROOT_PART_KB * 95 / 100)) ]; then
	echo "error: rootfs ($((ROOTFS_KB / 1024)) MiB) too large for ${ROOT_MIB} MiB ext4 partition" >&2
	echo "Increase IMAGE_MB= (currently ${IMAGE_MB} MiB total; ${ESP_MB} MiB is the UEFI boot partition)." >&2
	exit 1
fi

echo "Applying overlay + app tree (pack-time refresh) ..."
rsync -a "$SCRIPT_DIR/overlay/" "$ROOT/"
install_app_tree
find "$ROOT/etc" "$ROOT/opt/leap-netboot" -type f \
	\( -name '*.sh' -o -name '*.py' -o -name '*.conf' -o -path '*/init.d/*' \) \
	-exec sed -i 's/\r$//' {} \; 2>/dev/null || true

sed -i 's/^root:[^:]*:/root::/' "$ROOT/etc/shadow"

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
enable_svc boot networking
enable_svc default nginx
enable_svc default in.tftpd
enable_svc default leap-netboot
enable_svc default crond
enable_svc shutdown killprocs
enable_svc shutdown savecache
enable_svc shutdown mount-ro

build_pxe_grub
if [ -x "$NETBOOT_ROOT/scripts/render-pxe.sh" ]; then
	LEAP_NETBOOT_ROOT="$ROOT/var/lib/leap-netboot" \
		"$NETBOOT_ROOT/scripts/render-pxe.sh" || true
fi

resolve_boot_images

ROOT_FSUUID="$(uuidgen | tr '[:upper:]' '[:lower:]')"
KERNEL_ARGS="root=UUID=${ROOT_FSUUID} rootfstype=ext4 rootwait rootdelay=5 modules=sd-mod,ext4 earlyprintk=serial,ttyS0,115200 console=ttyS0,115200n8"

echo "Writing GRUB config into rootfs (root UUID ${ROOT_FSUUID}) ..."
mkdir -p "$ROOT/boot/grub"
cat > "$ROOT/boot/grub/grub.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
set default=0
set timeout=3

menuentry "LeapOS NetBoot Server (Alpine)" {
	search --no-floppy --fs-uuid --set=root ${ROOT_FSUUID}
	linux ${VMLINUZ} ${KERNEL_ARGS}
	initrd ${INITRD}
	boot
}
EOF

echo "Creating ${IMAGE_MB} MiB GPT disk (UEFI ESP + legacy BIOS + ext4 root) ..."
rm -f "$IMG"
truncate -s "$IMAGE_BYTES" "$IMG"

printf '%s\n' \
	'label: gpt' \
	'unit: sectors' \
	"start=${BIOS_BOOT_START}, size=${BIOS_BOOT_SECTORS}, type=${PART_TYPE_BIOS}" \
	"start=${ESP_START}, size=${ESP_SECTORS}, type=${PART_TYPE_ESP}" \
	"start=${ROOT_START}, size=${ROOT_SECTORS}, type=${PART_TYPE_LINUX}" |
	sfdisk --force "$IMG" >/dev/null

echo "Packing ext4 root (${ROOT_MIB} MiB) ..."
rm -f "$PART_IMG"
mke2fs -q -t ext4 -b 4096 -L LEAPNB -U "$ROOT_FSUUID" -d "$ROOT" "$PART_IMG" "$FS_BLOCKS"

dd if="$PART_IMG" of="$IMG" bs=512 seek="$ROOT_START" count="$ROOT_SECTORS" \
	conv=notrunc status=none
verify_disk_image "$IMG" "$ROOT_START" "$ROOT_SECTORS" "$FS_BLOCKS"

build_esp_grub

echo "Embedding GRUB for legacy BIOS boot (GPT bios_grub partition) ..."
cat > "$LINUX_WORK/early.cfg" <<EOF
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial console
terminal_output serial console
search --no-floppy --fs-uuid --set=root ${ROOT_FSUUID}
linux ${VMLINUZ} ${KERNEL_ARGS}
initrd ${INITRD}
boot
EOF

bios_mods="$(grub_pick_mods_from "$GRUB_BIOS_DIR" $GRUB_BIOS_MODS)"
# shellcheck disable=SC2086
grub-mkimage -O i386-pc -o "$LINUX_WORK/core.img" -d "$GRUB_BIOS_DIR" \
	--prefix='(hd0,gpt3)/boot/grub' \
	-c "$LINUX_WORK/early.cfg" \
	$bios_mods

core_bytes="$(wc -c < "$LINUX_WORK/core.img" | tr -d ' ')"
if [ "$core_bytes" -gt $((BIOS_BOOT_SECTORS * 512)) ]; then
	echo "error: GRUB core.img too large for bios_grub partition (${core_bytes} bytes)" >&2
	exit 1
fi

dd if="$GRUB_BIOS_DIR/boot.img" of="$IMG" conv=notrunc bs=446 count=1 status=none
dd if="$LINUX_WORK/core.img" of="$IMG" conv=notrunc bs=512 seek="$BIOS_BOOT_START" status=none
verify_disk_image "$IMG" "$ROOT_START" "$ROOT_SECTORS" "$FS_BLOCKS"

ls -lh "$IMG"
echo ""
echo "Done: $IMG"
echo "Boot: UEFI/legacy x86_64 — kernel/initrd on FAT ESP, serial COM1 @ 115200."
echo "Web UI: http://<server-ip>/"
echo "Configure router DHCP — see NetbootServer/docs/ROUTER-DHCP.md"
