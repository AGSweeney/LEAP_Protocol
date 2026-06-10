#!/bin/bash
# Cross-build OpENer for RTEMS pc386 and install libopener_rtems.a for the gateway link.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

BUILD_DIR="${OPENER_ROOT}/build-rtems-pc386"
TOOLCHAIN="${RTEMS_PREFIX}/bin/i386-rtems6-gcc"
RTEMS_LIBBSD_INC="${RTEMS_BSPS}/pc386/lib/include"
DEST_LIB="${RTEMS_BSPS}/pc386/lib"
OPENER_GATE="${OPENER_ROOT}/source/src/ports/RTEMS/opener_rtems_gate.h"
RTEMS_AR="${RTEMS_PREFIX}/bin/i386-rtems6-ar"
RTEMS_RANLIB="${RTEMS_PREFIX}/bin/i386-rtems6-ranlib"

if [ ! -x "$TOOLCHAIN" ]; then
	echo "RTEMS toolchain not found: $TOOLCHAIN" >&2
	exit 1
fi

if [ ! -d "${OPENER_ROOT}/source" ]; then
	echo "OpENer source not found at ${OPENER_ROOT}" >&2
	exit 1
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

RTEMS_CFLAGS="-I${RTEMS_LIBBSD_INC} -I${RTEMS_PREFIX}/i386-rtems6/pc386/lib/include -include ${OPENER_GATE}"

cmake "${OPENER_ROOT}/source" \
	-G Ninja \
	-DOpENer_PLATFORM=RTEMS \
	-DOpENer_TESTS=OFF \
	-DOPENER_INSTALL_AS_LIB=OFF \
	-DOpENer_BUILDSUPPORT_DIR="${OPENER_ROOT}/source/buildsupport" \
	-DOpENer_Device_Config_Device_Name="LeapOS-Gateway" \
	-DCMAKE_SYSTEM_NAME=Generic \
	-DCMAKE_C_COMPILER="$TOOLCHAIN" \
	-DCMAKE_CXX_COMPILER="${RTEMS_PREFIX}/bin/i386-rtems6-g++" \
	-DCMAKE_C_FLAGS="${RTEMS_CFLAGS}" \
	-DCMAKE_C_ARCHIVE_CREATE="<CMAKE_AR> qc <TARGET> <OBJECTS>" \
	-DCMAKE_C_ARCHIVE_APPEND="<CMAKE_AR> q <TARGET> <OBJECTS>" \
	-DCMAKE_C_ARCHIVE_FINISH="<CMAKE_RANLIB> <TARGET>" \
	-DCMAKE_FIND_ROOT_PATH="${RTEMS_BSPS}/pc386" \
	-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY

cmake --build .

MERGE_DIR="$(mktemp -d)"
trap 'rm -rf "$MERGE_DIR"' EXIT

OPENER_LIBS=(
	src/cip/libCIP.a
	src/enet_encap/libENET_ENCAP.a
	src/utils/libUtils.a
	src/ports/nvdata/libNVDATA.a
	src/ports/libPLATFORM_GENERIC.a
	src/ports/RTEMS/libRTEMSPLATFORM.a
	src/ports/RTEMS/sample_application/libSAMPLE_APP.a
)

for lib in "${OPENER_LIBS[@]}"; do
	if [ ! -f "${BUILD_DIR}/${lib}" ]; then
		echo "missing OpENer library: ${BUILD_DIR}/${lib}" >&2
		exit 1
	fi
	( cd "$MERGE_DIR" && "$RTEMS_AR" x "${BUILD_DIR}/${lib}" )
done

OBJ_COUNT="$(find "$MERGE_DIR" -name '*.obj' | wc -l)"
if [ "$OBJ_COUNT" -eq 0 ]; then
	echo "no OpENer object files extracted for merge" >&2
	exit 1
fi

MERGED="${DEST_LIB}/libopener_rtems.a"
rm -f "$MERGED"
( cd "$MERGE_DIR" && "$RTEMS_AR" crs "$MERGED" ./*.obj )
"$RTEMS_RANLIB" "$MERGED"

echo "Installed ${MERGED} (${OBJ_COUNT} objects)"
