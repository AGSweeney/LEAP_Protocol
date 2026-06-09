#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

if [ ! -d "$RTEMS_RSB" ]; then
    echo "RTEMS source tree missing at $RTEMS_RSB" >&2
    echo "Run: bash rtems-build/setup-rtems-tree.sh" >&2
    exit 1
fi

cd "$RTEMS_RSB"

exec ../source-builder/sb-set-builder \
  --prefix="$RTEMS_PREFIX" \
  --log="$RTEMS_ROOT/build/rsb-i386.log" \
  6/rtems-i386
