#!/bin/bash
# Cross-build OpENer as a static aarch64 library for LeapOS-Gateway Pi4.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
OPENER_ROOT="${OPENER_ROOT:-/mnt/d/OpENer-Enhanced}"
OPENER_SOURCE="$OPENER_ROOT/source"
STAGE_DIR="${LEAP_OPENER_STAGE_DIR:-$SCRIPT_DIR/build-work/opener-source}"
BUILD_DIR="${LEAP_OPENER_BUILD_DIR:-$SCRIPT_DIR/build-work/opener-aarch64}"
OUT_LIB="${LEAP_OPENER_LIB:-$SCRIPT_DIR/build-work/libopener_linux_aarch64.a}"
ENV_FILE="${LEAP_OPENER_ENV:-$SCRIPT_DIR/build-work/opener-aarch64.env}"

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
if ! command -v rsync >/dev/null 2>&1; then
	echo "error: missing command: rsync" >&2
	echo "Install: sudo apt install -y rsync" >&2
	exit 1
fi

if [ ! -f "$OPENER_SOURCE/CMakeLists.txt" ]; then
	echo "error: OpENer source not found at $OPENER_SOURCE" >&2
	echo "Set OPENER_ROOT=/path/to/OpENer-Enhanced" >&2
	exit 1
fi

echo "Staging OpENer source from: $OPENER_SOURCE"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
rsync -a --delete \
	--exclude '/build/' \
	--exclude '/build-*/' \
	"$OPENER_SOURCE/" "$STAGE_DIR/"

mkdir -p "$STAGE_DIR/src/ports/linux_port" "$STAGE_DIR/buildsupport/linux_port"
rsync -a --delete "$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapGateway-linux/opener/linux_port/" \
	"$STAGE_DIR/src/ports/linux_port/"
install -m 644 "$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapGateway-linux/opener/OpENer_PLATFORM_INCLUDES.cmake" \
	"$STAGE_DIR/buildsupport/linux_port/OpENer_PLATFORM_INCLUDES.cmake"
install -m 644 "$REPO_ROOT/platforms/x86-32/D945GSEJT/LeapGateway-linux/opener/opener_linux_gate.h" \
	"$STAGE_DIR/src/ports/linux_port/opener_linux_gate.h"

cmake \
	-S "$STAGE_DIR" \
	-B "$BUILD_DIR" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SYSTEM_NAME=Linux \
	-DCMAKE_C_COMPILER="$CC" \
	-DOpENer_PLATFORM=linux_port \
	-DOpENer_TESTS=OFF \
	-DOpENer_Device_Config_Vendor_Id="${OPENER_DEVICE_VENDOR_ID:-1}" \
	-DOpENer_Device_Config_Device_Type="${OPENER_DEVICE_TYPE:-12}" \
	-DOpENer_Device_Config_Product_Code="${OPENER_DEVICE_PRODUCT_CODE:-65001}" \
	-DOpENer_Device_Config_Device_Name="${OPENER_DEVICE_NAME:-LeapOS-Gateway}" \
	-DCIP_FILE_OBJECT=OFF \
	-DCIP_SECURITY_OBJECTS=OFF \
	-DCMAKE_C_FLAGS="-include $STAGE_DIR/src/ports/linux_port/opener_linux_gate.h"

cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 2)"

mapfile -t LIBS < <(find "$BUILD_DIR/src" -name 'lib*.a' | sort)
if [ "${#LIBS[@]}" -eq 0 ]; then
	echo "error: no OpENer static libraries produced under $BUILD_DIR/src" >&2
	exit 1
fi

rm -f "$OUT_LIB" "$BUILD_DIR/combine-opener.mri"
{
	echo "CREATE $OUT_LIB"
	for lib in "${LIBS[@]}"; do
		echo "ADDLIB $lib"
	done
	echo "SAVE"
	echo "END"
} > "$BUILD_DIR/combine-opener.mri"

"${AR:-${CC%-gcc}-ar}" -M < "$BUILD_DIR/combine-opener.mri"

cat > "$ENV_FILE" <<EOF
LEAP_GATEWAY_OPENER_LIBRARY='$OUT_LIB'
LEAP_GATEWAY_OPENER_INCLUDE_DIRS='$STAGE_DIR/src/ports/linux_port;$STAGE_DIR/src/ports/linux_port/sample_application;$STAGE_DIR/src;$STAGE_DIR/src/cip;$STAGE_DIR/src/enet_encap;$STAGE_DIR/src/ports;$STAGE_DIR/src/utils;$STAGE_DIR/src/cip_objects;$STAGE_DIR/src/ports/nvdata;$BUILD_DIR/src/ports'
EOF

file "$OUT_LIB" || true
echo "Installed: $OUT_LIB"
echo "Wrote:     $ENV_FILE"
