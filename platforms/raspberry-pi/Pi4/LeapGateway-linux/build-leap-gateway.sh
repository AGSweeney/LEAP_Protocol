#!/bin/bash
# Cross-build the real LeapOS-Gateway daemon for Alpine aarch64 Pi4.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
BUILD_DIR="${LEAP_GATEWAY_BUILD_DIR:-$SCRIPT_DIR/build-work/gateway-aarch64}"
OUT="$SCRIPT_DIR/alpine/overlay/usr/sbin/leap-gateway"

if [ "${HOSTTYPE:-}" = "aarch64" ] || [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	CC_DEFAULT="gcc"
else
	CC_DEFAULT="aarch64-linux-gnu-gcc"
fi

CC="${CC:-$CC_DEFAULT}"
if ! command -v "$CC" >/dev/null 2>&1; then
	echo "error: missing compiler: $CC" >&2
	echo "Install: sudo apt install -y gcc-aarch64-linux-gnu cmake make" >&2
	exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
	echo "error: missing command: cmake" >&2
	echo "Install: sudo apt install -y cmake" >&2
	exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
	echo "error: missing command: python3" >&2
	echo "Install: sudo apt install -y python3" >&2
	exit 1
fi

"$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapGateway/scripts/gen-web-index.sh"

OPENER_ENABLE="${LEAP_GATEWAY_OPENER_ENABLE:-0}"
OPENER_ENV="$SCRIPT_DIR/build-work/opener-aarch64.env"
CMAKE_ARGS=(
	-S "$SCRIPT_DIR"
	-B "$BUILD_DIR"
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_SYSTEM_NAME=Linux
	-DCMAKE_C_COMPILER="$CC"
	-DCMAKE_EXE_LINKER_FLAGS=-static
	-DLEAP_GATEWAY_OPENER_ENABLE="$OPENER_ENABLE"
)

if [ "$OPENER_ENABLE" = "1" ]; then
	if [ -z "${LEAP_GATEWAY_OPENER_LIBRARY:-}" ]; then
		echo "LEAP_GATEWAY_OPENER_ENABLE=1: building OpENer for aarch64 ..."
		OPENER_ROOT="${OPENER_ROOT:-/mnt/d/OpENer-Enhanced}" "$SCRIPT_DIR/build-opener-linux.sh"
		if [ -f "$OPENER_ENV" ]; then
			# shellcheck disable=SC1090
			. "$OPENER_ENV"
		fi
	fi
	if [ -z "${LEAP_GATEWAY_OPENER_LIBRARY:-}" ]; then
		echo "error: OpENer build did not set LEAP_GATEWAY_OPENER_LIBRARY" >&2
		exit 1
	fi
	CMAKE_ARGS+=(
		-DLEAP_GATEWAY_OPENER_LIBRARY="$LEAP_GATEWAY_OPENER_LIBRARY"
		-DLEAP_GATEWAY_OPENER_INCLUDE_DIRS="${LEAP_GATEWAY_OPENER_INCLUDE_DIRS:-}"
	)
else
	echo "note: building without OpENer/EtherNet/IP; set LEAP_GATEWAY_OPENER_ENABLE=1 after building OpENer for aarch64"
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --target leap-gateway -j"$(nproc 2>/dev/null || echo 2)"

mkdir -p "$(dirname "$OUT")"
install -m 755 "$BUILD_DIR/leap-gateway" "$OUT"
file "$OUT" || true
echo "Installed: $OUT"
