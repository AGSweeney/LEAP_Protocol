#!/bin/bash
# Build static aarch64 LEAP tools (AF_PACKET) and install them into the Alpine
# overlay so mk-image.sh packs them into the Pi 4 gateway image.
#
# Host (x86_64): sudo apt install -y gcc-aarch64-linux-gnu cmake
# Host (aarch64): sudo apt install -y gcc cmake
# Static linking → no glibc/musl mismatch on the Alpine aarch64 target.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../../" && pwd)"
BUILD_DIR="${LEAP_TOOLS_BUILD_DIR:-/tmp/leap-tools-aarch64}"
DEST="$SCRIPT_DIR/overlay/usr/sbin"

if [ ! -f "$REPO_ROOT/CMakeLists.txt" ] || [ ! -d "$REPO_ROOT/leap_core" ]; then
	echo "error: repo root not found from $SCRIPT_DIR (got $REPO_ROOT)" >&2
	exit 1
fi

HOST_ARCH="$(uname -m)"
if [ "$HOST_ARCH" = "aarch64" ] || [ "$HOST_ARCH" = "arm64" ]; then
	CC="${CC:-gcc}"
	CFLAGS="-O2"
	LDFLAGS="-static"
else
	CC="${CC:-aarch64-linux-gnu-gcc}"
	CFLAGS="-O2"
	LDFLAGS="-static"
fi

for cmd in cmake "$CC"; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd" >&2
		if [ "$HOST_ARCH" != "aarch64" ] && [ "$HOST_ARCH" != "arm64" ]; then
			echo "Install cross compiler: sudo apt install -y gcc-aarch64-linux-gnu cmake" >&2
		else
			echo "Install: sudo apt install -y gcc cmake" >&2
		fi
		exit 1
	fi
done

if ! echo 'int main(void){return 0;}' | "$CC" -static -x c - -o /tmp/leap-a64-check 2>/dev/null; then
	echo "error: $CC cannot link -static" >&2
	exit 1
fi
rm -f /tmp/leap-a64-check

echo "=== Building LEAP tools (static aarch64, CC=$CC) ==="
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER="$CC" \
	-DCMAKE_C_FLAGS="$CFLAGS" \
	-DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
	>/dev/null

TARGETS=(leap_linux_discover leap_linux_controller leap_linux_hub leap_linux_device leap_tests)
cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target "${TARGETS[@]}"

mkdir -p "$DEST"
install -m 755 "$BUILD_DIR/leap_linux_discover"   "$DEST/leap-discover"
install -m 755 "$BUILD_DIR/leap_linux_controller" "$DEST/leap-controller"
install -m 755 "$BUILD_DIR/leap_linux_hub"        "$DEST/leap-hub"
install -m 755 "$BUILD_DIR/leap_linux_device"     "$DEST/leap-device"
install -m 755 "$BUILD_DIR/leap_tests"            "$DEST/leap-selftest"
strip "$DEST/leap-discover" "$DEST/leap-controller" "$DEST/leap-hub" \
	"$DEST/leap-device" "$DEST/leap-selftest"

echo ""
echo "Installed into overlay (repack with: sudo bash mk-image.sh):"
ls -lh "$DEST"/leap-*
