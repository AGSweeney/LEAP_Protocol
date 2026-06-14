#!/bin/bash
# Cross-build Linux AF_PACKET LEAP tools for Alpine aarch64 Pi4.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
BUILD_DIR="${LEAP_TOOLS_BUILD_DIR:-$SCRIPT_DIR/build-work/leap-tools-aarch64}"
OVERLAY_BIN="$SCRIPT_DIR/alpine/overlay/usr/sbin"

if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
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

cmake \
	-S "$REPO_ROOT" \
	-B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SYSTEM_NAME=Linux \
	-DCMAKE_C_COMPILER="$CC" \
	-DCMAKE_EXE_LINKER_FLAGS=-static

cmake --build "$BUILD_DIR" --target \
	leap_linux_discover leap_linux_controller leap_linux_hub leap_tests \
	-j"$(nproc 2>/dev/null || echo 2)"

mkdir -p "$OVERLAY_BIN"
install -m 755 "$BUILD_DIR/leap_linux_discover" "$OVERLAY_BIN/leap-discover"
install -m 755 "$BUILD_DIR/leap_linux_controller" "$OVERLAY_BIN/leap-controller"
install -m 755 "$BUILD_DIR/leap_linux_hub" "$OVERLAY_BIN/leap-hub"
install -m 755 "$BUILD_DIR/leap_tests" "$OVERLAY_BIN/leap-selftest"

file "$OVERLAY_BIN"/leap-{discover,controller,hub,selftest} || true
echo "Installed LEAP tools into: $OVERLAY_BIN"
