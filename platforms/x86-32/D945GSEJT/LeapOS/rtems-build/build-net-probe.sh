#!/bin/bash
# Build LeapOS net-probe.exe inside rtems-libbsd (correct libbsd linking).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

LIBBSD_A="$RTEMS_BSPS/pc386/lib/libbsd.a"
TEST_NAME="leapos-net-probe"
TEST_DIR="testsuite/$TEST_NAME"

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
    echo "Run: bash rtems-build/setup-libbsd.sh" >&2
    exit 1
fi

LIBBSD_SRC="$(dirname "$LIBBSD_WSCRIPT")"
LIBBSD_PY="$LIBBSD_SRC/libbsd.py"
REGISTER_LINE="        self.addTest(mm.generator['test']('$TEST_NAME', ['test_main']))"

echo "Using libbsd source: $LIBBSD_SRC"

mkdir -p "$LIBBSD_SRC/$TEST_DIR"
cp "$NET_PROBE_DIR/test_main.c" "$LIBBSD_SRC/$TEST_DIR/test_main.c"

if ! grep -q "$TEST_NAME" "$LIBBSD_PY"; then
    echo "Registering $TEST_NAME in libbsd.py"
    sed -i "/self.addTest(mm.generator\['test'\]('loopback01'/a\\$REGISTER_LINE" "$LIBBSD_PY"
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

mkdir -p "$(dirname "$NET_PROBE_EXE")"
cp "$BUILT_EXE" "$NET_PROBE_EXE"

ls -lh "$NET_PROBE_EXE"
echo "net-probe.exe ready"
