#!/bin/bash
# Produce pc386 config.ini with run-once behavior (halt after app exit, no reboot).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

SRC="${1:-$RTEMS_ROOT/build/config.ini}"
OUT="${2:-$RTEMS_ROOT/build/config-runonce.ini}"

if [ ! -f "$SRC" ]; then
    echo "Missing base config: $SRC" >&2
    echo "Regenerate: cd ~/rtems/src/rtems && ./waf bspdefaults --rtems-bsps=i386/pc386 > ~/rtems/build/config.ini" >&2
    exit 1
fi

# Guard against corrupted config (USE_VGA=True fails to compile on RTEMS 6.2).
if grep -q '^USE_VGA = True' "$SRC"; then
    echo "Base config has USE_VGA=True (broken). Regenerating from bspdefaults..." >&2
    if [ ! -f "$RTEMS_SRC/rtems/wscript" ]; then
        echo "Cannot regenerate — RTEMS source missing." >&2
        exit 1
    fi
    (cd "$RTEMS_SRC/rtems" && ./waf bspdefaults --rtems-bsps=i386/pc386 > "$SRC")
fi

mkdir -p "$(dirname "$OUT")"
cp "$SRC" "$OUT"

sed -i 's/^BSP_RESET_BOARD_AT_EXIT = .*/BSP_RESET_BOARD_AT_EXIT = 0/' "$OUT"
sed -i 's/^BSP_PRESS_KEY_FOR_RESET = .*/BSP_PRESS_KEY_FOR_RESET = 0/' "$OUT"
sed -i 's/^RTEMS_BUILD_LABEL = .*/RTEMS_BUILD_LABEL = LeapOS-D945GSEJT/' "$OUT"
sed -i 's/^RTEMS_POSIX_API = .*/RTEMS_POSIX_API = True/' "$OUT"
sed -i 's/^BSP_VERBOSE_FATAL_EXTENSION = .*/BSP_VERBOSE_FATAL_EXTENSION = 0/' "$OUT"

echo "Run-once config: $OUT"
grep -E '^(BSP_RESET_BOARD_AT_EXIT|BSP_PRESS_KEY_FOR_RESET|RTEMS_BUILD_LABEL|RTEMS_POSIX_API|BSP_VERBOSE_FATAL_EXTENSION)' "$OUT"
