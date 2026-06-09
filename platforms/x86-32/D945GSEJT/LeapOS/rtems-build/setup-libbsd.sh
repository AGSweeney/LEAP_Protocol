#!/bin/bash
# Build and install RTEMS libbsd for i386/pc386 (FreeBSD 14 / re driver).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

LIBBSD_A="$RTEMS_BSPS/pc386/lib/libbsd.a"
LOG="${RTEMS_ROOT}/build/rsb-libbsd-i386.log"

if [ -f "$LIBBSD_A" ] && [ "${LEAP_FORCE_LIBBSD:-0}" != "1" ]; then
    echo "libbsd already installed: $LIBBSD_A"
    ls -lh "$LIBBSD_A"
    exit 0
fi

if [ ! -d "$RTEMS_RSB" ]; then
    echo "RTEMS RSB tree missing at $RTEMS_RSB" >&2
    echo "Run: bash rtems-build/setup-rtems-tree.sh" >&2
    exit 1
fi

if [ ! -x "$RTEMS_TOOLS/i386-rtems6-gcc" ]; then
    echo "RTEMS i386 toolchain missing at $RTEMS_TOOLS" >&2
    exit 1
fi

mkdir -p "$RTEMS_ROOT/build"

echo "Building rtems-libbsd for $RTEMS_BSP (log: $LOG)"
echo "This can take 30–90 minutes on first run."

cd "$RTEMS_RSB"

set +e
../source-builder/sb-set-builder \
    --prefix="$RTEMS_PREFIX" \
    --with-rtems-bsp="$RTEMS_BSP" \
    --log="$LOG" \
    6/rtems-libbsd-fb14
rsb_status=$?
set -e

if [ ! -f "$LIBBSD_A" ]; then
    build_dir="$(find "$RTEMS_ROOT" "$RTEMS_SRC" -path '*/rtems-libbsd-6.2/build/i386-rtems6-pc386-default/libbsd.a' 2>/dev/null | head -1)"
    if [ -n "$build_dir" ] && [ -f "$build_dir" ]; then
        echo "RSB failed on libbsd tests; installing library from partial build"
        libbsd_root="$(cd "$(dirname "$build_dir")/../.." && pwd)"
        (cd "$libbsd_root" && ./waf install) || true
        if [ ! -f "$LIBBSD_A" ]; then
            mkdir -p "$(dirname "$LIBBSD_A")"
            cp "$build_dir" "$LIBBSD_A"
        fi
    elif [ "$rsb_status" -ne 0 ]; then
        echo "error: libbsd RSB build failed (exit $rsb_status)" >&2
        echo "See log: $LOG" >&2
        exit 1
    fi
fi

if [ ! -f "$LIBBSD_A" ]; then
    echo "error: libbsd build finished but $LIBBSD_A not found" >&2
    exit 1
fi

ls -lh "$LIBBSD_A"
echo "libbsd ready for $RTEMS_BSP"
