#!/bin/bash
# Stage LeapOS-Device Alpine PXE boot tree (kernel + initramfs + modloop + GRUB).
#   bash make-pxe-device-alpine.sh              # use cached rootfs
#   bash make-pxe-device-alpine.sh --build      # rebuild leap-device + repack rootfs overlay
#
# Output: LeapOS/rtems-image/pxe-device-alpine/
# Publish: NetbootServer/scripts/publish-leapos-device-alpine.sh
set -euo pipefail

_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LEAPOS_ROOT="$(cd "$DEVICE_ROOT/../LeapOS" && pwd)"
IMAGE_DIR="${LEAPOS_IMAGE_DIR:-$LEAPOS_ROOT/rtems-image}"

DO_BUILD=0
PXE_PREFIX="${PXE_PREFIX:-leapos-device-alpine}"
PXE_OUT="${LEAPOS_PXE_DEVICE_ALPINE_DIR:-$IMAGE_DIR/pxe-device-alpine}"
GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"
WORK="$SCRIPT_DIR/build-work"
ROOT="$WORK/rootfs"
ROOTFS_STAMP="$ROOT/.leap-alpine-rootfs-ready"
OVERLAY_LEAP_DEVICE="$SCRIPT_DIR/overlay/usr/sbin/leap-device"

ALPINE_RELEASE="${ALPINE_RELEASE:-3.20.6}"
ALPINE_BRANCH="${ALPINE_BRANCH:-$(echo "$ALPINE_RELEASE" | cut -d. -f1,2)}"
ALPINE_REPO="${ALPINE_REPO:-http://dl-cdn.alpinelinux.org/alpine/v${ALPINE_BRANCH}/main}"
LEAP_PXE_HTTP_BASE="${LEAP_PXE_HTTP_BASE:-http://REPLACE_WITH_NETBOOT_SERVER/httpboot/${PXE_PREFIX}}"

VMLINUZ="${VMLINUZ:-vmlinuz-lts}"
INITRAMFS="${INITRAMFS:-initramfs-lts}"
MODLOOP="${MODLOOP:-modloop-lts}"
APKOVL="${APKOVL:-leap-device.apkovl.tar.gz}"

KERNEL_APPEND_BASE="ip=dhcp alpine_repo=${ALPINE_REPO} modules=loop,squashfs,sd-mod,ext4,r8169 earlyprintk=serial,ttyS0,115200 console=tty1 console=ttyS0,115200n8"

while [ $# -gt 0 ]; do
	case "$1" in
	--build) DO_BUILD=1; shift ;;
	--http-base)
		LEAP_PXE_HTTP_BASE="$2"
		shift 2
		;;
	-h|--help)
		echo "Usage: $0 [--build] [--http-base URL]" >&2
		echo "  LEAP_PXE_HTTP_BASE  HTTP base for modloop (default: placeholder)" >&2
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

need_cmd grub-mkimage cp rsync mksquashfs find

if [ ! -f "$GRUB_DIR/boot.img" ]; then
	echo "error: GRUB i386-pc not found at $GRUB_DIR" >&2
	echo "Install: sudo apt install -y grub-pc-bin squashfs-tools" >&2
	exit 1
fi

if [ "$DO_BUILD" = "1" ]; then
	bash "$DEVICE_ROOT/build-leap-device.sh"
	bash "$SCRIPT_DIR/mk-image.sh"
fi

if [ ! -f "$ROOTFS_STAMP" ] || [ ! -f "$ROOT/boot/$VMLINUZ" ] || [ ! -f "$ROOT/boot/$INITRAMFS" ]; then
	echo "error: Alpine rootfs not ready at $ROOT" >&2
	echo "Run: cd $DEVICE_ROOT && bash build-leap-device.sh && bash alpine/mk-image.sh" >&2
	echo "First rootfs build: sudo FORCE_ROOTFS=1 bash alpine/mk-image.sh" >&2
	exit 1
fi

MODLOOP_URL="${LEAP_PXE_HTTP_BASE%/}/${MODLOOP}"
APKOVL_URL="${LEAP_PXE_HTTP_BASE%/}/${APKOVL}"
KERNEL_APPEND="${KERNEL_APPEND_BASE} modloop=${MODLOOP_URL} apkovl=${APKOVL_URL}"

echo "=== LeapOS-Device Alpine PXE staging ==="
echo "Output:  $PXE_OUT"
echo "Prefix:  /${PXE_PREFIX}"
echo "Modloop: $MODLOOP_URL"
echo "Apkovl:  $APKOVL_URL"

MODLOOP_WORK="$(mktemp -d "${TMPDIR:-/tmp}/leap-alpine-modloop.XXXXXX")"
APKOVL_WORK="$(mktemp -d "${TMPDIR:-/tmp}/leap-alpine-apkovl.XXXXXX")"
trap 'rm -rf "$MODLOOP_WORK" "$APKOVL_WORK"' EXIT

echo "Building modloop (Alpine modules/ layout, gzip — matches official netboot) ..."
KVER="$(ls -1 "$ROOT/lib/modules" 2>/dev/null | grep -v -E 'microsoft|WSL' | head -1)"
if [ -z "$KVER" ] || [ ! -d "$ROOT/lib/modules/$KVER" ]; then
	echo "error: kernel modules not found under $ROOT/lib/modules" >&2
	exit 1
fi
mkdir -p "$MODLOOP_WORK/modules"
cp -a "$ROOT/lib/modules/$KVER" "$MODLOOP_WORK/modules/"

rm -rf "$PXE_OUT"
mkdir -p "$PXE_OUT/boot/grub/i386-pc"

cp "$ROOT/boot/$VMLINUZ" "$PXE_OUT/"
cp "$ROOT/boot/$INITRAMFS" "$PXE_OUT/"
mksquashfs "$MODLOOP_WORK" "$PXE_OUT/$MODLOOP" -comp gzip -noappend >/dev/null
echo "modloop: modules/$KVER ($(du -h "$PXE_OUT/$MODLOOP" | awk '{print $1}'))"

echo "Building apkovl (diskless boot services + leap-device for PXE RAM root) ..."
LEAP_DEVICE_BIN="$ROOT/usr/sbin/leap-device"
if [ -x "$OVERLAY_LEAP_DEVICE" ]; then
	LEAP_DEVICE_BIN="$OVERLAY_LEAP_DEVICE"
fi
if [ ! -x "$LEAP_DEVICE_BIN" ]; then
	echo "error: missing leap-device binary — run build-leap-device.sh first" >&2
	exit 1
fi
mkdir -p \
	"$APKOVL_WORK/etc/local.d" \
	"$APKOVL_WORK/etc/network" \
	"$APKOVL_WORK/etc/runlevels/boot" \
	"$APKOVL_WORK/etc/runlevels/default" \
	"$APKOVL_WORK/etc/runlevels/shutdown" \
	"$APKOVL_WORK/etc/runlevels/sysinit" \
	"$APKOVL_WORK/usr/sbin"
# Trigger initramfs rc_add (modloop sysinit, hostname boot, …). Without this,
# a hand-built apkovl skips diskless defaults and /lib/modules never mounts.
: > "$APKOVL_WORK/etc/.default_boot_services"
cat > "$APKOVL_WORK/etc/fstab" <<'EOF'
tmpfs  /   tmpfs  defaults,size=256m  0 0
tmpfs  /tmp tmpfs  defaults,size=16m   0 0
EOF
printf '%s\n' 'leapos-device' > "$APKOVL_WORK/etc/hostname"
cat > "$APKOVL_WORK/etc/network/interfaces" <<'EOF'
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet dhcp
EOF
if [ -f "$ROOT/etc/passwd" ]; then
	cp "$ROOT/etc/passwd" "$APKOVL_WORK/etc/passwd"
fi
if [ -f "$ROOT/etc/group" ]; then
	cp "$ROOT/etc/group" "$APKOVL_WORK/etc/group"
fi
if [ -f "$ROOT/etc/shadow" ]; then
	cp "$ROOT/etc/shadow" "$APKOVL_WORK/etc/shadow"
	sed -i 's/^root:[^:]*:/root::/' "$APKOVL_WORK/etc/shadow"
	chmod 600 "$APKOVL_WORK/etc/shadow"
fi
cp "$LEAP_DEVICE_BIN" "$APKOVL_WORK/usr/sbin/leap-device"
chmod 755 "$APKOVL_WORK/usr/sbin/leap-device"
cat > "$APKOVL_WORK/etc/local.d/leap-device.start" <<'EOF'
#!/bin/sh
# PXE netboot: make the NIC usable after pivot, then start LEAP device.
modprobe r8169 2>/dev/null || true
for _try in 1 2 3 4 5; do
	[ -d /sys/class/net/eth0 ] && break
	sleep 1
done
if [ -d /sys/class/net/eth0 ]; then
	ip link set eth0 up 2>/dev/null || ifconfig eth0 up 2>/dev/null || true
	if ! ip -4 addr show dev eth0 2>/dev/null | grep -q 'inet '; then
		udhcpc -i eth0 -q -t 5 -n 2>/dev/null || true
	fi
fi
if [ -x /usr/sbin/leap-device ]; then
	/usr/sbin/leap-device &
fi
EOF
chmod 755 "$APKOVL_WORK/etc/local.d/leap-device.start"

for svc in devfs dmesg mdev hwdrivers modloop; do
	ln -sf "/etc/init.d/$svc" "$APKOVL_WORK/etc/runlevels/sysinit/$svc"
done
for svc in modules sysctl hostname bootmisc networking; do
	ln -sf "/etc/init.d/$svc" "$APKOVL_WORK/etc/runlevels/boot/$svc"
done
ln -sf /etc/init.d/local "$APKOVL_WORK/etc/runlevels/default/local"
for svc in killprocs savecache mount-ro; do
	ln -sf "/etc/init.d/$svc" "$APKOVL_WORK/etc/runlevels/shutdown/$svc"
done

find "$APKOVL_WORK" -type f -exec sed -i 's/\r$//' {} \;
tar -czf "$PXE_OUT/$APKOVL" -C "$APKOVL_WORK" .

sed "s|@KERNEL_APPEND@|${KERNEL_APPEND}|g" \
	"$SCRIPT_DIR/grub/leapos-device-alpine-pxe-grub.cfg" > "$PXE_OUT/boot/grub/grub.cfg"

pxe_mods="$(grub_pick_mods \
	pxe tftp net linux serial terminal gzio normal configfile echo \
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

mkdir -p "$PXE_OUT/ipxe"
cat > "$PXE_OUT/ipxe/leapos-device-alpine.ipxe" <<EOF
#!ipxe
echo LeapOS-Device Alpine PXE
dhcp
kernel http://\${next-server}/httpboot/${PXE_PREFIX}/${VMLINUZ} ${KERNEL_APPEND}
initrd http://\${next-server}/httpboot/${PXE_PREFIX}/${INITRAMFS}
boot
EOF

cat > "$PXE_OUT/README.txt" <<EOF
LeapOS-Device Alpine PXE tree
=============================

Copy this directory under your TFTP root (e.g. /var/lib/leap-netboot/tftp/).

Legacy BIOS PXE (DHCP option 67):
  ${PXE_PREFIX}/boot/grub/i386-pc/core.0

iPXE (recommended — HTTP kernel/initrd/modloop):
  chain http://<server>/httpboot/${PXE_PREFIX}/ipxe/leapos-device-alpine.ipxe

NetBoot Server publish:
  ../../../../NetbootServer/scripts/publish-leapos-device-alpine.sh \\
    ${PXE_OUT} --name "LeapOS device Alpine" --default

Web UI: upload ${IMAGE_DIR}/leap-device-alpine-pxe.tar.gz at http://<netboot-server>/

If using GRUB TFTP only, set modloop URL when staging:
  LEAP_PXE_HTTP_BASE=http://<server>/httpboot/${PXE_PREFIX} bash make-pxe-device-alpine.sh

Serial: COM1 115200 8N1
EOF

ls -lh "$PXE_OUT/$VMLINUZ" "$PXE_OUT/$INITRAMFS" "$PXE_OUT/$MODLOOP" \
	"$PXE_OUT/$APKOVL" "$PXE_OUT/boot/grub/i386-pc/core.0"
du -sh "$PXE_OUT"

BUNDLE="$IMAGE_DIR/leap-device-alpine-pxe.tar.gz"
tar -czf "$BUNDLE" -C "$PXE_OUT" "$VMLINUZ" "$INITRAMFS" "$MODLOOP" "$APKOVL"
echo "Web UI upload bundle: $BUNDLE ($(du -h "$BUNDLE" | cut -f1))"

cat <<EOF

Alpine PXE tree ready: $PXE_OUT

Standalone TFTP:
  dhcp option 67: ${PXE_PREFIX}/boot/grub/i386-pc/core.0

NetBoot Server:
  Web UI — upload leap-device-alpine-pxe.tar.gz (Images → Alpine Linux)
  Or: ../../../../NetbootServer/scripts/publish-leapos-device-alpine.sh \\
    "$PXE_OUT" --name "LeapOS device Alpine" --default
  Remote API: publish-leapos-device-alpine.sh "$PXE_OUT" --api http://<server> --default
EOF
