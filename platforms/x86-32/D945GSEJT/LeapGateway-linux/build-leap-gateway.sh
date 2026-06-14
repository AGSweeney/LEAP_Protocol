#!/bin/bash
# Build the full LeapOS-Gateway daemon (LEAP session hub + EIP bridge + Web UI)
# as a static i386 Linux binary and install it into the Alpine overlay as
# /usr/sbin/leap-gateway (replacing the shell stub).
#
# Shares application sources in ../LeapGateway/src — the Linux platform layer
# (transport, net, storage, main) lives in ./src.
#
# Host: WSL/Ubuntu. Needs: sudo apt install -y gcc-multilib python3
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
GATEWAY_DIR="$SCRIPT_DIR/../LeapGateway"
LEAPPORT_DIR="$SCRIPT_DIR/../LeapPort"
BUILD_DIR="${LEAP_GATEWAY_BUILD_DIR:-/tmp/leap-gateway-i386}"
DEST="$SCRIPT_DIR/alpine/overlay/usr/sbin"

if [ ! -d "$REPO_ROOT/leap_core" ] || [ ! -d "$GATEWAY_DIR/src" ]; then
	echo "error: repo layout not found from $SCRIPT_DIR" >&2
	exit 1
fi

for cmd in gcc python3; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd — install: sudo apt install -y gcc-multilib python3" >&2
		exit 1
	fi
done

if ! echo 'int main(void){return 0;}' | gcc -m32 -static -x c - -o /tmp/leap-m32-check 2>/dev/null; then
	echo "error: gcc cannot link -m32 -static — install: sudo apt install -y gcc-multilib" >&2
	exit 1
fi
rm -f /tmp/leap-m32-check

echo "=== Regenerating embedded web UI ==="
bash "$GATEWAY_DIR/scripts/gen-web-index.sh"

echo "=== Regenerating build info ==="
bash "$LEAPPORT_DIR/scripts/gen_build_info.sh"

OPENER_LIB="$SCRIPT_DIR/build-work/libopener_linux.a"
OPENER_INC="$SCRIPT_DIR/opener/linux_port"

echo "=== Building OpENer (static i386) ==="
bash "$SCRIPT_DIR/build-opener-linux.sh"

if [ ! -f "$OPENER_LIB" ]; then
	echo "error: OpENer library missing: $OPENER_LIB" >&2
	exit 1
fi

# i686 baseline covers Atom N270; static so the binary is self-contained on musl.
CFLAGS="-m32 -march=i686 -mtune=generic -O2 -Wall -pthread"
LDFLAGS="-m32 -static -pthread"
DEFINES="-DLEAP_GATEWAY_OPENER_ENABLE=1"

# Linux platform headers (leap_transport.h / leap_time.h / leap_config.h) must
# shadow the RTEMS ones — keep $SCRIPT_DIR/src first.
INCLUDES=(
	-I"$SCRIPT_DIR/src"
	-I"$GATEWAY_DIR/src"
	-I"$OPENER_INC"
	-I"$REPO_ROOT/leap_core/inc"
	-I"$LEAPPORT_DIR/generated"
)

LINUX_SRC=(
	"$SCRIPT_DIR/src/gateway_main_linux.c"
	"$SCRIPT_DIR/src/gateway_net_linux.c"
	"$SCRIPT_DIR/src/gateway_storage_linux.c"
	"$SCRIPT_DIR/src/leap_transport_linux.c"
	"$SCRIPT_DIR/src/leap_time_linux.c"
)

SHARED_SRC=(
	"$GATEWAY_DIR/src/gateway_global.c"
	"$GATEWAY_DIR/src/gateway_http.c"
	"$GATEWAY_DIR/src/gateway_leap_session.c"
	"$GATEWAY_DIR/src/gateway_rtems_io.c"
	"$GATEWAY_DIR/src/gateway_pd_io.c"
	"$GATEWAY_DIR/src/gateway_web_index.c"
	"$GATEWAY_DIR/src/leap_gateway_opener.c"
)

CORE_SRC=(
	"$REPO_ROOT/leap_core/src/crc/leap_crc.c"
	"$REPO_ROOT/leap_core/src/frame/leap_frame.c"
	"$REPO_ROOT/leap_core/src/services/disc/leap_disc_controller.c"
	"$REPO_ROOT/leap_core/src/services/dir/leap_dir_controller.c"
	"$REPO_ROOT/leap_core/src/services/dir/leap_dir_controller_capabilities.c"
	"$REPO_ROOT/leap_core/src/services/mgmt/leap_mgmt_controller.c"
	"$REPO_ROOT/leap_core/src/services/mgmt/leap_mgmt_process.c"
	"$REPO_ROOT/leap_core/src/services/mgmt/leap_mgmt_device.c"
	"$REPO_ROOT/leap_core/src/services/pd/leap_pd_controller.c"
	"$REPO_ROOT/leap_core/src/services/pd/leap_pd_common.c"
	"$REPO_ROOT/leap_core/src/services/diag/leap_diag_controller.c"
	"$REPO_ROOT/leap_core/src/leap_controller_stack.c"
	"$REPO_ROOT/leap_core/src/leap_controller_session_hub.c"
	"$REPO_ROOT/leap_core/src/leap_controller_peer.c"
	"$REPO_ROOT/leap_core/src/leap_controller_sequence.c"
	"$REPO_ROOT/leap_core/src/bridge/leap_eip_bridge.c"
	"$REPO_ROOT/leap_core/src/bridge/leap_gateway_config.c"
	"$REPO_ROOT/leap_core/src/leap_log.c"
	"$REPO_ROOT/leap_core/src/leap_build_info.c"
	"$REPO_ROOT/leap_core/src/transport/leap_raw_linux.c"
)

echo "=== Building leap-gateway (static i386) ==="
mkdir -p "$BUILD_DIR"
OBJS=()
for src in "${LINUX_SRC[@]}" "${SHARED_SRC[@]}" "${CORE_SRC[@]}"; do
	obj="$BUILD_DIR/$(basename "${src%.c}").o"
	gcc $CFLAGS $DEFINES "${INCLUDES[@]}" -c "$src" -o "$obj"
	OBJS+=("$obj")
done

gcc $LDFLAGS "${OBJS[@]}" "$OPENER_LIB" -o "$BUILD_DIR/leap-gateway"
strip "$BUILD_DIR/leap-gateway"

mkdir -p "$DEST"
install -m 755 "$BUILD_DIR/leap-gateway" "$DEST/leap-gateway"

echo ""
echo "Installed into overlay (repack with: sudo bash alpine/mk-image.sh):"
ls -lh "$DEST/leap-gateway"
