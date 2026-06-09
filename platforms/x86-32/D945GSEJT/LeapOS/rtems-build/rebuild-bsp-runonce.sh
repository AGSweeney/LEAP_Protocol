#!/bin/bash
# Rebuild pc386 BSP with run-once + POSIX (required by libbsd / net-probe).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

RTEMS_TREE="$RTEMS_SRC/rtems"
CONFIG="$RTEMS_ROOT/build/config-runonce.ini"

if [ ! -f "$RTEMS_TREE/wscript" ]; then
    echo "RTEMS source not found at $RTEMS_TREE" >&2
    exit 1
fi

bash "$SCRIPT_DIR/apply-runonce-config.sh"

cd "$RTEMS_TREE"
rm -f .lock-waf_linux_build 2>/dev/null || true

echo "Configuring pc386 (run-once, no reset at exit)..."
./waf configure \
    --prefix="$RTEMS_PREFIX" \
    --rtems-tools="$RTEMS_PREFIX" \
    --rtems-bsps=i386/pc386 \
    --rtems-config="$CONFIG"

echo "Building and installing pc386 BSP..."
./waf build install

echo "Run-once BSP installed to $RTEMS_PREFIX"
