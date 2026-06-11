#!/bin/bash
# Build OpENer as a static i386 library for LeapOS-Gateway on Linux.
#
# Stages OpENer-Enhanced source, injects the LINUX platform port from ./opener/,
# and merges all static libs into libopener_linux.a.
#
# Host: WSL/Ubuntu. Needs: cmake ninja-build gcc-multilib
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPENER_ROOT="${OPENER_ROOT:-/mnt/d/OpENer-Enhanced}"
BUILD_DIR="${LEAP_OPENER_BUILD_DIR:-/tmp/leap-opener-i386}"
STAGE_DIR="$BUILD_DIR/opener-src"
GATE_H="$SCRIPT_DIR/opener/opener_linux_gate.h"
OUT_LIB="$SCRIPT_DIR/build-work/libopener_linux.a"

if [ ! -d "${OPENER_ROOT}/source" ]; then
	echo "error: OpENer source not found at ${OPENER_ROOT}" >&2
	echo "Set OPENER_ROOT to your OpENer-Enhanced checkout." >&2
	exit 1
fi

for cmd in cmake ninja gcc ar ranlib rsync; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: missing $cmd — install: sudo apt install -y cmake ninja-build gcc-multilib" >&2
		exit 1
	fi
done

if ! echo 'int main(void){return 0;}' | gcc -m32 -static -x c - -o /tmp/leap-m32-check 2>/dev/null; then
	echo "error: gcc cannot link -m32 -static — install: sudo apt install -y gcc-multilib" >&2
	exit 1
fi
rm -f /tmp/leap-m32-check

echo "=== Staging OpENer source ==="
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
rsync -a --delete \
	--exclude 'build-*' \
	"${OPENER_ROOT}/source/" "$STAGE_DIR/"

echo "=== Injecting LINUX platform port ==="
rsync -a "$SCRIPT_DIR/opener/linux_port/" "$STAGE_DIR/src/ports/LINUX/"
mkdir -p "$STAGE_DIR/buildsupport/LINUX"
cp "$SCRIPT_DIR/opener/OpENer_PLATFORM_INCLUDES.cmake" "$STAGE_DIR/buildsupport/LINUX/"

LINUX_CFLAGS="-m32 -march=i686 -mtune=generic -O2 -include ${GATE_H} -DCIP_FILE_OBJECT=0 -DCIP_SECURITY_OBJECTS=0"

echo "=== Configuring OpENer (LINUX, static i386) ==="
cmake -S "$STAGE_DIR" -B "$BUILD_DIR/cmake" \
	-G Ninja \
	-DOpENer_PLATFORM=LINUX \
	-DOpENer_TESTS=OFF \
	-DOpENer_TRACES=OFF \
	-DOPENER_INSTALL_AS_LIB=OFF \
	-DOpENer_BUILDSUPPORT_DIR="$STAGE_DIR/buildsupport" \
	-DOpENer_Device_Config_Device_Name="LeapOS-Gateway" \
	-DOpENer_CIP_OBJECT_CIP_FILE_OBJECT=OFF \
	-DCIP_FILE_OBJECT=OFF \
	-DCIP_SECURITY_OBJECTS=OFF \
	-DOPENER_IS_DLR_DEVICE=OFF \
	-DOPENER_LLDP=OFF \
	-DCMAKE_C_FLAGS="$LINUX_CFLAGS" \
	-DCMAKE_EXE_LINKER_FLAGS="-m32 -static"

cmake --build "$BUILD_DIR/cmake"

MERGE_DIR="$(mktemp -d)"
trap 'rm -rf "$MERGE_DIR"' EXIT

OPENER_LIBS=(
	src/cip/libCIP.a
	src/enet_encap/libENET_ENCAP.a
	src/utils/libUtils.a
	src/ports/nvdata/libNVDATA.a
	src/ports/libPLATFORM_GENERIC.a
	src/ports/LINUX/libLINUXPLATFORM.a
	src/ports/LINUX/sample_application/libSAMPLE_APP.a
)

for lib in "${OPENER_LIBS[@]}"; do
	if [ ! -f "${BUILD_DIR}/cmake/${lib}" ]; then
		echo "error: missing OpENer library: ${BUILD_DIR}/cmake/${lib}" >&2
		exit 1
	fi
	( cd "$MERGE_DIR" && ar x "${BUILD_DIR}/cmake/${lib}" )
done

OBJ_COUNT="$(find "$MERGE_DIR" -name '*.o' | wc -l)"
if [ "$OBJ_COUNT" -eq 0 ]; then
	echo "error: no OpENer object files extracted for merge" >&2
	exit 1
fi

mkdir -p "$(dirname "$OUT_LIB")"
rm -f "$OUT_LIB"
( cd "$MERGE_DIR" && ar crs "$OUT_LIB" ./*.o )
ranlib "$OUT_LIB"

echo ""
echo "Installed ${OUT_LIB} (${OBJ_COUNT} objects)"
