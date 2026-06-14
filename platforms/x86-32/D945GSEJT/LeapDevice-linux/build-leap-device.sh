#!/bin/bash
# Build the LEAP device daemon as a static i386 Linux binary and install it
# into the Alpine overlay as /usr/sbin/leap-device.
#
# Host: WSL/Ubuntu. Needs: sudo apt install -y gcc-multilib
_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
LEAPPORT_DIR="$SCRIPT_DIR/../LeapPort"
GATEWAY_LINUX="$SCRIPT_DIR/../LeapGateway-linux"
BUILD_DIR="${LEAP_DEVICE_BUILD_DIR:-/tmp/leap-device-i386}"
DEST="$SCRIPT_DIR/alpine/overlay/usr/sbin"

if [ ! -d "$REPO_ROOT/leap_core" ] || [ ! -d "$LEAPPORT_DIR/src" ]; then
	echo "error: repo layout not found from $SCRIPT_DIR" >&2
	exit 1
fi

for cmd in gcc; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd — install: sudo apt install -y gcc-multilib" >&2
		exit 1
	fi
done

if ! echo 'int main(void){return 0;}' | gcc -m32 -static -x c - -o /tmp/leap-m32-check 2>/dev/null; then
	echo "error: gcc cannot link -m32 -static — install: sudo apt install -y gcc-multilib" >&2
	exit 1
fi
rm -f /tmp/leap-m32-check

echo "=== Regenerating build info ==="
bash "$LEAPPORT_DIR/scripts/gen_build_info.sh"

CFLAGS="-m32 -march=i686 -mtune=generic -O2 -Wall"
LDFLAGS="-m32 -static"

INCLUDES=(
	-I"$SCRIPT_DIR/src"
	-I"$REPO_ROOT/leap_core/inc"
	-I"$LEAPPORT_DIR/generated"
)

LINUX_SRC=(
	"$SCRIPT_DIR/src/device_main_linux.c"
	"$SCRIPT_DIR/src/device_net_linux.c"
	"$SCRIPT_DIR/src/leap_board_linux.c"
	"$GATEWAY_LINUX/src/leap_transport_linux.c"
	"$GATEWAY_LINUX/src/leap_time_linux.c"
)

CORE_SRC=(
	"$REPO_ROOT/leap_core/src/crc/leap_crc.c"
	"$REPO_ROOT/leap_core/src/frame/leap_frame.c"
	"$REPO_ROOT/leap_core/src/services/disc/leap_disc_device.c"
	"$REPO_ROOT/leap_core/src/services/dir/leap_dir_device.c"
	"$REPO_ROOT/leap_core/src/services/mgmt/leap_mgmt_device.c"
	"$REPO_ROOT/leap_core/src/services/mgmt/leap_mgmt_process.c"
	"$REPO_ROOT/leap_core/src/services/pd/leap_pd_device.c"
	"$REPO_ROOT/leap_core/src/services/pd/leap_pd_common.c"
	"$REPO_ROOT/leap_core/src/services/diag/leap_diag_device.c"
	"$REPO_ROOT/leap_core/src/leap_device_stack.c"
	"$REPO_ROOT/leap_core/src/leap_log.c"
	"$REPO_ROOT/leap_core/src/leap_build_info.c"
	"$REPO_ROOT/leap_core/src/transport/leap_raw_linux.c"
)

echo "=== Building leap-device (static i386) ==="
mkdir -p "$BUILD_DIR"
OBJS=()
for src in "${LINUX_SRC[@]}" "${CORE_SRC[@]}"; do
	obj="$BUILD_DIR/$(basename "${src%.c}").o"
	gcc $CFLAGS "${INCLUDES[@]}" -c "$src" -o "$obj"
	OBJS+=("$obj")
done

gcc $LDFLAGS "${OBJS[@]}" -o "$BUILD_DIR/leap-device"
strip "$BUILD_DIR/leap-device"

mkdir -p "$DEST"
install -m 755 "$BUILD_DIR/leap-device" "$DEST/leap-device"

echo ""
echo "Installed into overlay (repack with: sudo bash alpine/mk-image.sh):"
ls -lh "$DEST/leap-device"
