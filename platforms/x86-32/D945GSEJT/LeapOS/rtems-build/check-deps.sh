#!/bin/bash
# Verify host packages needed to build LeapOS boot images (WSL/Linux).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

missing=()

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        missing+=("$1")
    fi
}

need_cmd bash
need_cmd gcc
need_cmd g++
need_cmd make
need_cmd python3
need_cmd wget
need_cmd tar
need_cmd grub-mkrescue
need_cmd xorriso
need_cmd mkfs.vfat
need_cmd sfdisk
need_cmd losetup

if [ "${#missing[@]}" -gt 0 ]; then
    echo "Missing commands: ${missing[*]}" >&2
    echo "Install on Ubuntu/WSL:" >&2
    echo "  sudo apt install build-essential python3 python3-venv wget tar \\"
    echo "    grub-pc-bin grub-common xorriso dosfstools fdisk util-linux" >&2
    exit 1
fi

if [ ! -x "$RTEMS_TOOLS/i386-rtems6-gcc" ]; then
    echo "RTEMS i386 toolchain not found at $RTEMS_TOOLS" >&2
    echo "Run: bash rtems-build/setup-rtems-tree.sh && bash rtems-build/rsb-build.sh" >&2
    exit 1
fi

echo "Dependencies OK (RTEMS prefix: $RTEMS_PREFIX)"
