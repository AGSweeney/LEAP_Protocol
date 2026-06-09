#!/bin/bash
# Build LeapOS leap-port.exe (LEAP discovery MVP) inside rtems-libbsd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

LIBBSD_A="$RTEMS_BSPS/pc386/lib/libbsd.a"
TEST_NAME="leapos-leap-port"
TEST_DIR="testsuite/$TEST_NAME"

LEAP_CORE_SOURCES=(
	leap_crc
	leap_frame
	leap_disc_device
	leap_dir_device
	leap_mgmt_device
	leap_mgmt_process
	leap_pd_device
	leap_pd_common
	leap_diag_device
	leap_device_stack
	leap_log
	leap_build_info
)

PORT_SOURCES=(
	init
	leap_transport
	leap_board
	leap_time
)

if [ ! -f "$LIBBSD_A" ]; then
	echo "libbsd not found at $LIBBSD_A" >&2
	echo "Run: bash rtems-build/setup-libbsd.sh" >&2
	exit 1
fi

find_libbsd_src() {
	find "$RTEMS_ROOT" "$RTEMS_SRC" -path '*/rtems-libbsd-6.2/wscript' 2>/dev/null | head -1
}

LIBBSD_WSCRIPT="$(find_libbsd_src)"
if [ -z "$LIBBSD_WSCRIPT" ]; then
	echo "rtems-libbsd source tree not found under $RTEMS_ROOT or $RTEMS_SRC" >&2
	exit 1
fi

LIBBSD_SRC="$(dirname "$LIBBSD_WSCRIPT")"
LIBBSD_PY="$LIBBSD_SRC/libbsd.py"

SOURCE_LIST=""
for src in "${PORT_SOURCES[@]}" "${LEAP_CORE_SOURCES[@]}"; do
	if [ -n "$SOURCE_LIST" ]; then
		SOURCE_LIST+=", "
	fi
	SOURCE_LIST+="'$src'"
done

REGISTER_LINE="        self.addTest(mm.generator['test']('$TEST_NAME', [$SOURCE_LIST]))"

echo "Using libbsd source: $LIBBSD_SRC"

mkdir -p "$LIBBSD_SRC/$TEST_DIR"

cp "$LEAP_PORT_DIR/src/init.c" "$LIBBSD_SRC/$TEST_DIR/init.c"
cp "$LEAP_PORT_DIR/src/leap_transport.c" "$LIBBSD_SRC/$TEST_DIR/leap_transport.c"
cp "$LEAP_PORT_DIR/src/leap_board.c" "$LIBBSD_SRC/$TEST_DIR/leap_board.c"
cp "$LEAP_PORT_DIR/src/leap_time.c" "$LIBBSD_SRC/$TEST_DIR/leap_time.c"
cp "$LEAP_PORT_DIR/src/leap_config.h" "$LIBBSD_SRC/$TEST_DIR/leap_config.h"
cp "$LEAP_PORT_DIR/src/leap_board.h" "$LIBBSD_SRC/$TEST_DIR/leap_board.h"
cp "$LEAP_PORT_DIR/src/leap_transport.h" "$LIBBSD_SRC/$TEST_DIR/leap_transport.h"
cp "$LEAP_PORT_DIR/src/leap_time.h" "$LIBBSD_SRC/$TEST_DIR/leap_time.h"

cp "$REPO_ROOT/leap_core/inc/leap/"*.h "$LIBBSD_SRC/$TEST_DIR/"
cp "$LEAP_PORT_DIR/generated/leap_build_info_gen.h" \
	"$LIBBSD_SRC/$TEST_DIR/leap_build_info_gen.h"
if [ -d "$REPO_ROOT/leap_core/inc/leap/conformance" ]; then
	cp -r "$REPO_ROOT/leap_core/inc/leap/conformance" \
		"$LIBBSD_SRC/$TEST_DIR/conformance"
fi

# libbsd tests compile with the source directory as an include root; flatten
# leap/*.h includes so nested headers resolve (GCC searches from the includer).
find "$LIBBSD_SRC/$TEST_DIR" \( -name '*.c' -o -name '*.h' \) -exec \
	sed -i 's/#include "leap\//#include "/g' {} +

for src in "${LEAP_CORE_SOURCES[@]}"; do
	case "$src" in
	leap_crc) from=src/crc/leap_crc.c ;;
	leap_frame) from=src/frame/leap_frame.c ;;
	leap_disc_device) from=src/services/disc/leap_disc_device.c ;;
	leap_dir_device) from=src/services/dir/leap_dir_device.c ;;
	leap_mgmt_device) from=src/services/mgmt/leap_mgmt_device.c ;;
	leap_mgmt_process) from=src/services/mgmt/leap_mgmt_process.c ;;
	leap_pd_device) from=src/services/pd/leap_pd_device.c ;;
	leap_pd_common) from=src/services/pd/leap_pd_common.c ;;
	leap_diag_device) from=src/services/diag/leap_diag_device.c ;;
	leap_device_stack) from=src/leap_device_stack.c ;;
	leap_log) from=src/leap_log.c ;;
	leap_build_info) from=src/leap_build_info.c ;;
	esac
	cp "$REPO_ROOT/leap_core/$from" "$LIBBSD_SRC/$TEST_DIR/${src}.c"
done

find "$LIBBSD_SRC/$TEST_DIR" -name '*.c' -exec \
	sed -i 's/#include "leap\//#include "/g' {} +

if grep -q "$TEST_NAME" "$LIBBSD_PY"; then
	echo "Updating $TEST_NAME source list in libbsd.py"
	sed -i "s#^[[:space:]]*self.addTest(mm.generator\\['test'\\]('$TEST_NAME'.*\$#$REGISTER_LINE#" "$LIBBSD_PY"
else
	echo "Registering $TEST_NAME in libbsd.py"
	sed -i "/self.addTest(mm.generator\['test'\]('leapos-net-probe'/a\\$REGISTER_LINE" "$LIBBSD_PY"
fi

cd "$LIBBSD_SRC"

bash "$SCRIPT_DIR/apply-libbsd-fxp-patches.sh"
bash "$SCRIPT_DIR/apply-libbsd-nexus-patches.sh"
bash "$SCRIPT_DIR/apply-libbsd-re-patches.sh"

if [ ! -f "build/config.log" ]; then
	./waf configure \
		--prefix="$RTEMS_PREFIX" \
		--rtems-tools="$RTEMS_PREFIX" \
		--rtems-bsp="$RTEMS_BSP" \
		--rtems-version="$RTEMS_VERSION"
fi

./waf build --targets="${TEST_NAME}.exe"

BUILT_EXE="$LIBBSD_SRC/build/i386-rtems6-pc386-default/${TEST_NAME}.exe"
if [ ! -f "$BUILT_EXE" ]; then
	echo "error: ${TEST_NAME}.exe not produced at $BUILT_EXE" >&2
	exit 1
fi

mkdir -p "$(dirname "$LEAP_PORT_EXE")"
cp "$BUILT_EXE" "$LEAP_PORT_EXE"

ls -lh "$LEAP_PORT_EXE"
echo "leap-port.exe ready"
