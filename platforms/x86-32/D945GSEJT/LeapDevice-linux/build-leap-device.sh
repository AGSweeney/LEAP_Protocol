#!/bin/bash
# Build the LEAP device daemon as a static i386 Linux binary and install it
# into the Alpine overlay as /usr/sbin/leap-device.
#
# Host: WSL/Ubuntu. Needs: sudo apt install -y gcc-multilib
#
# Board flavors (LEAP_BOARD):
#   lpt        — D945GSEJT LPT1 8x8 (default)
#   mcc_dio24  — Measurement Computing PCI-DIO-24H 16x8
_self="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
sed -i 's/\r$//' "$_self" 2>/dev/null || true
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
LEAPPORT_DIR="$SCRIPT_DIR/../LeapPort"
GATEWAY_LINUX="$SCRIPT_DIR/../LeapGateway-linux"
BUILD_DIR="${LEAP_DEVICE_BUILD_DIR:-/tmp/leap-device-i386}"
DEST="$SCRIPT_DIR/alpine/overlay/usr/sbin"
LEAP_BOARD="${LEAP_BOARD:-lpt}"

if [ ! -d "$REPO_ROOT/leap_core" ] || [ ! -d "$LEAPPORT_DIR/src" ]; then
	echo "error: repo layout not found from $SCRIPT_DIR" >&2
	exit 1
fi

case "$LEAP_BOARD" in
lpt)
	BOARD_SRC="$SCRIPT_DIR/src/leap_board_linux.c"
	CONFIG_HDR="$SCRIPT_DIR/src/leap_config.h"
	;;
mcc_dio24)
	BOARD_SRC="$SCRIPT_DIR/src/leap_board_mcc_dio24.c"
	CONFIG_HDR="$SCRIPT_DIR/src/leap_config_mcc_dio24.h"
	;;
*)
	echo "error: unknown LEAP_BOARD=$LEAP_BOARD (expected lpt or mcc_dio24)" >&2
	exit 1
	;;
esac

for cmd in gcc; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd — install: sudo apt install -y gcc-multilib" >&2
		exit 1
	fi
done

M32_CFLAGS=(-m32 -march=i686 -mtune=generic)
if [ ! -d /usr/include/i386-linux-gnu ] && [ -d /usr/include/x86_64-linux-gnu ]; then
	# Ubuntu/WSL multilib: gcc -m32 looks for i386-linux-gnu headers that are not
	# installed as a separate tree; the 32-bit stubs live under x86_64-linux-gnu.
	M32_CFLAGS+=(-I/usr/include/x86_64-linux-gnu)
fi

if ! echo '#include <errno.h>
int main(void){return errno;}' | gcc "${M32_CFLAGS[@]}" -static -x c - -o /tmp/leap-m32-check 2>/dev/null; then
	echo "error: gcc cannot build -m32 -static — install: sudo apt install -y gcc-multilib libc6-dev-i386" >&2
	exit 1
fi
rm -f /tmp/leap-m32-check

echo "=== Regenerating build info ==="
bash "$LEAPPORT_DIR/scripts/gen_build_info.sh"

CFLAGS="${M32_CFLAGS[*]} -O2 -Wall -DLEAP_BOARD_${LEAP_BOARD}=1"
LDFLAGS="-m32 -static"

INCLUDES=(
	-I"$SCRIPT_DIR/src"
	-I"$SCRIPT_DIR/src/drivers"
	-I"$REPO_ROOT/leap_core/inc"
	-I"$LEAPPORT_DIR/generated"
)

LINUX_SRC=(
	"$SCRIPT_DIR/src/device_main_linux.c"
	"$SCRIPT_DIR/src/device_net_linux.c"
	"$BOARD_SRC"
	"$GATEWAY_LINUX/src/leap_transport_linux.c"
	"$GATEWAY_LINUX/src/leap_time_linux.c"
)

if [ "$LEAP_BOARD" = "mcc_dio24" ]; then
	LINUX_SRC+=("$SCRIPT_DIR/src/drivers/mcc_pci_dio24h.c")
fi

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

echo "=== Building leap-device (static i386, board=$LEAP_BOARD) ==="
mkdir -p "$BUILD_DIR"
cp "$CONFIG_HDR" "$BUILD_DIR/leap_config.h"
OBJS=()
for src in "${LINUX_SRC[@]}" "${CORE_SRC[@]}"; do
	obj="$BUILD_DIR/$(basename "${src%.c}").o"
	gcc $CFLAGS "${INCLUDES[@]}" -I"$BUILD_DIR" -c "$src" -o "$obj"
	OBJS+=("$obj")
done

gcc $LDFLAGS "${OBJS[@]}" -o "$BUILD_DIR/leap-device"
strip "$BUILD_DIR/leap-device"

mkdir -p "$DEST"
install -m 755 "$BUILD_DIR/leap-device" "$DEST/leap-device"

if [ "$LEAP_BOARD" = "mcc_dio24" ]; then
	echo "=== Building mcc-dio24 CLI (static i386) ==="
	CLI_OBJ="$BUILD_DIR/mcc_dio24_cli.o"
	DRIVER_OBJ="$BUILD_DIR/mcc_pci_dio24h.o"
	gcc $CFLAGS "${INCLUDES[@]}" -c "$SCRIPT_DIR/src/mcc_dio24_cli.c" -o "$CLI_OBJ"
	gcc $LDFLAGS "$CLI_OBJ" "$DRIVER_OBJ" -o "$BUILD_DIR/mcc-dio24"
	strip "$BUILD_DIR/mcc-dio24"
	install -m 755 "$BUILD_DIR/mcc-dio24" "$DEST/mcc-dio24"
fi

echo ""
echo "Installed into overlay (repack with: sudo bash alpine/mk-image.sh):"
ls -lh "$DEST/leap-device"
if [ "$LEAP_BOARD" = "mcc_dio24" ]; then
	ls -lh "$DEST/mcc-dio24"
fi
