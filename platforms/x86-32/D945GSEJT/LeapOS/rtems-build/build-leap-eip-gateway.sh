#!/bin/bash
# Build LeapOS leap-eip-gateway.exe (LeapOS-Gateway) inside rtems-libbsd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

LIBBSD_A="$RTEMS_BSPS/pc386/lib/libbsd.a"
TEST_NAME="leapos-leap-eip-gateway"
TEST_DIR="testsuite/$TEST_NAME"

LEAP_CORE_SOURCES=(
	leap_crc
	leap_frame
	leap_disc_controller
	leap_dir_controller
	leap_dir_controller_capabilities
	leap_mgmt_controller
	leap_mgmt_process
	leap_pd_controller
	leap_pd_common
	leap_diag_controller
	leap_controller_stack
	leap_controller_peer
	leap_controller_sequence
	leap_eip_bridge
	leap_gateway_config
	leap_log
	leap_build_info
)

GATEWAY_SOURCES=(
	gateway_init
	gateway_global
	gateway_leap_session
	gateway_net
	gateway_rtems_io
	gateway_pd_io
	gateway_http
	gateway_storage
	gateway_web_index
	leap_gateway_opener
	leap_transport
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
for src in "${GATEWAY_SOURCES[@]}" "${LEAP_CORE_SOURCES[@]}"; do
	if [ -n "$SOURCE_LIST" ]; then
		SOURCE_LIST+=", "
	fi
	SOURCE_LIST+="'$src'"
done

REGISTER_LINE="        self.addTest(mm.generator['test']('$TEST_NAME', [$SOURCE_LIST], extraLibs=['opener_rtems']))"

echo "Using libbsd source: $LIBBSD_SRC"

bash "$LEAP_GATEWAY_DIR/scripts/gen-web-index.sh"

bash "$SCRIPT_DIR/build-opener-rtems.sh"

mkdir -p "$LIBBSD_SRC/$TEST_DIR"

cp "$LEAP_GATEWAY_DIR/src/"*.c "$LIBBSD_SRC/$TEST_DIR/"
cp "$LEAP_GATEWAY_DIR/src/"*.h "$LIBBSD_SRC/$TEST_DIR/"
cp "$LEAP_PORT_DIR/src/leap_config.h" "$LIBBSD_SRC/$TEST_DIR/leap_config.h"
cp "$LEAP_PORT_DIR/src/leap_transport.c" "$LIBBSD_SRC/$TEST_DIR/leap_transport.c"
cp "$LEAP_PORT_DIR/src/leap_transport.h" "$LIBBSD_SRC/$TEST_DIR/leap_transport.h"
cp "$LEAP_PORT_DIR/src/leap_time.c" "$LIBBSD_SRC/$TEST_DIR/leap_time.c"
cp "$LEAP_PORT_DIR/src/leap_time.h" "$LIBBSD_SRC/$TEST_DIR/leap_time.h"
cp "$LEAP_PORT_DIR/generated/leap_build_info_gen.h" \
	"$LIBBSD_SRC/$TEST_DIR/leap_build_info_gen.h"
cp "$OPENER_ROOT/source/src/ports/RTEMS/opener.h" \
	"$LIBBSD_SRC/$TEST_DIR/opener.h"

cp "$REPO_ROOT/leap_core/inc/leap/"*.h "$LIBBSD_SRC/$TEST_DIR/"
if [ -d "$REPO_ROOT/leap_core/inc/leap/conformance" ]; then
	cp -r "$REPO_ROOT/leap_core/inc/leap/conformance" \
		"$LIBBSD_SRC/$TEST_DIR/conformance"
fi

find "$LIBBSD_SRC/$TEST_DIR" \( -name '*.c' -o -name '*.h' \) -exec \
	sed -i 's/#include "leap\//#include "/g' {} +

copy_leap_core() {
	local src="$1"
	local from="$2"
	cp "$REPO_ROOT/leap_core/$from" "$LIBBSD_SRC/$TEST_DIR/${src}.c"
}

copy_leap_core leap_crc src/crc/leap_crc.c
copy_leap_core leap_frame src/frame/leap_frame.c
copy_leap_core leap_disc_controller src/services/disc/leap_disc_controller.c
copy_leap_core leap_dir_controller src/services/dir/leap_dir_controller.c
copy_leap_core leap_dir_controller_capabilities src/services/dir/leap_dir_controller_capabilities.c
copy_leap_core leap_mgmt_controller src/services/mgmt/leap_mgmt_controller.c
copy_leap_core leap_mgmt_process src/services/mgmt/leap_mgmt_process.c
copy_leap_core leap_pd_controller src/services/pd/leap_pd_controller.c
copy_leap_core leap_pd_common src/services/pd/leap_pd_common.c
copy_leap_core leap_diag_controller src/services/diag/leap_diag_controller.c
copy_leap_core leap_controller_stack src/leap_controller_stack.c
copy_leap_core leap_controller_peer src/leap_controller_peer.c
copy_leap_core leap_controller_sequence src/leap_controller_sequence.c
copy_leap_core leap_eip_bridge src/bridge/leap_eip_bridge.c
copy_leap_core leap_gateway_config src/bridge/leap_gateway_config.c
copy_leap_core leap_log src/leap_log.c
copy_leap_core leap_build_info src/leap_build_info.c

find "$LIBBSD_SRC/$TEST_DIR" -name '*.c' -exec \
	sed -i 's/#include "leap\//#include "/g' {} +

if grep -q "$TEST_NAME" "$LIBBSD_PY"; then
	echo "Updating $TEST_NAME source list in libbsd.py"
	sed -i "s#^[[:space:]]*self.addTest(mm.generator\\['test'\\]('$TEST_NAME'.*\$#$REGISTER_LINE#" "$LIBBSD_PY"
else
	echo "Registering $TEST_NAME in libbsd.py"
	sed -i "/self.addTest(mm.generator\['test'\]('leapos-leap-port'/a\\$REGISTER_LINE" "$LIBBSD_PY"
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

mkdir -p "$(dirname "$LEAP_GATEWAY_EXE")"
cp "$BUILT_EXE" "$LEAP_GATEWAY_EXE"

ls -lh "$LEAP_GATEWAY_EXE"
echo "leap-eip-gateway.exe ready (LeapOS-Gateway)"
