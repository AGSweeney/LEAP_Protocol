#!/bin/bash
# Build static i386 LEAP tools (AF_PACKET) and install them into the Alpine
# overlay so mk-image.sh packs them into the gateway image.
#
# Host: WSL/Ubuntu. Needs: sudo apt install -y gcc-multilib cmake
# Static linking → no glibc/musl mismatch on the Alpine i386 target.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
BUILD_DIR="${LEAP_TOOLS_BUILD_DIR:-/tmp/leap-tools-i386}"
DEST="$SCRIPT_DIR/overlay/usr/sbin"

if [ ! -f "$REPO_ROOT/CMakeLists.txt" ] || [ ! -d "$REPO_ROOT/leap_core" ]; then
	echo "error: repo root not found from $SCRIPT_DIR (got $REPO_ROOT)" >&2
	exit 1
fi

for cmd in cmake gcc; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd — install: sudo apt install -y gcc-multilib cmake" >&2
		exit 1
	fi
done

if ! echo 'int main(void){return 0;}' | gcc -m32 -static -x c - -o /tmp/leap-m32-check 2>/dev/null; then
	echo "error: gcc cannot link -m32 -static — install: sudo apt install -y gcc-multilib" >&2
	exit 1
fi
rm -f /tmp/leap-m32-check

# i686 baseline covers Atom N270; static so the binary is self-contained on musl.
CFLAGS="-m32 -march=i686 -mtune=generic -O2"
LDFLAGS="-m32 -static"

echo "=== Building LEAP tools (static i386) ==="
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=Release \
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
