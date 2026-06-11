#!/bin/bash
# Rebuild RTEMS pc386 BSP with D945GSEJT VGA-friendly config (USE_VBE_RM=false).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

CONFIG="$SCRIPT_DIR/config/config-d945gsejt.ini"
RTEMS_TREE="$RTEMS_SRC/rtems"

if [ ! -f "$CONFIG" ]; then
    echo "Missing $CONFIG" >&2
    exit 1
fi

if [ ! -f "$RTEMS_TREE/wscript" ]; then
    echo "RTEMS source not found at $RTEMS_TREE" >&2
    echo "Run: bash rtems-build/setup-rtems-tree.sh" >&2
    exit 1
fi

mkdir -p "$RTEMS_ROOT/build"
cp "$CONFIG" "$RTEMS_ROOT/build/config.ini"

cd "$RTEMS_TREE"
rm -f .lock-waf_linux_build 2>/dev/null || true

echo "Applying CF/IDE LBA patch for CompactFlash..."
bash "$SCRIPT_DIR/apply-rtems-ide-cf-patch.sh"

echo "Configuring pc386 with D945GSEJT profile..."
./waf configure \
    --prefix="$RTEMS_PREFIX" \
    --rtems-tools="$RTEMS_PREFIX" \
    --rtems-bsps=i386/pc386 \
    --rtems-config="$CONFIG"

echo "Building and installing pc386 BSP (this takes several minutes)..."
./waf build install

echo "BSP install complete: $RTEMS_PREFIX"
