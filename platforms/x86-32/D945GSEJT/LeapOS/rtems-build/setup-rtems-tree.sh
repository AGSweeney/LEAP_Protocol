#!/bin/bash
# One-time fetch of RTEMS 6.2 source trees under ~/rtems/src (WSL/Linux).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

RTEMS_RELEASE="${RTEMS_RELEASE:-6.2}"
RSB_URL="${RSB_URL:-https://git.rtems.org/rtems-source-builder/snapshot/rtems-source-builder-${RTEMS_RELEASE}.tar.bz2}"
RTEMS_URL="${RTEMS_URL:-https://git.rtems.org/rtems/snapshot/rtems-${RTEMS_RELEASE}.tar.bz2}"
EXAMPLES_URL="${EXAMPLES_URL:-https://git.rtems.org/rtems-examples/snapshot/rtems-examples-${RTEMS_RELEASE}.tar.bz2}"

mkdir -p "$RTEMS_ROOT/build" "$RTEMS_SRC"

fetch_extract() {
    local url="$1"
    local dest="$2"
    local name
    name="$(basename "$url")"
    local work="$RTEMS_ROOT/build/$name"

    if [ -d "$dest" ]; then
        echo "skip (exists): $dest"
        return 0
    fi

    echo "Fetching $url"
    wget -O "$work" "$url"
    mkdir -p "$dest"
    tar -xjf "$work" -C "$dest" --strip-components=1
    rm -f "$work"
}

# RSB layout expected by rsb-build.sh: $RTEMS_SRC/rsb/rtems + source-builder sibling
if [ ! -x "$RTEMS_SRC/rsb/source-builder/sb-set-builder" ]; then
    mkdir -p "$RTEMS_SRC/rsb"
    fetch_extract "$RSB_URL" "$RTEMS_SRC/rsb/source-builder"
fi

if [ ! -f "$RTEMS_RSB/wscript" ] && [ ! -d "$RTEMS_RSB/config" ]; then
    mkdir -p "$RTEMS_SRC/rsb"
    fetch_extract "$RTEMS_URL" "$RTEMS_RSB"
fi

if [ ! -f "$RTEMS_EXAMPLES/wscript" ]; then
    fetch_extract "$EXAMPLES_URL" "$RTEMS_EXAMPLES"
fi

echo "RTEMS source tree ready under $RTEMS_SRC"
echo "Next: bash rtems-build/rsb-build.sh   (hours — builds toolchain + pc386 BSP)"
