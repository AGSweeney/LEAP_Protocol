#!/bin/bash
# Populate rtems-image/staging with boot payloads + GRUB config.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

if [ ! -f "$LEAP_PORT_EXE" ]; then
	echo "error: leap-port.exe not found at $LEAP_PORT_EXE" >&2
	echo "Run: bash rtems-build/build-leap-port.sh" >&2
	exit 1
fi

mkdir -p "$LEAPOS_STAGING/boot/grub"
cp "$LEAP_PORT_EXE" "$LEAPOS_STAGING/leap-port.exe"

if [ -f "$NET_PROBE_EXE" ]; then
	cp "$NET_PROBE_EXE" "$LEAPOS_STAGING/net-probe.exe"
fi

echo "Staged leap-port.exe"

cp "$SCRIPT_DIR/grub/leapos-grub.cfg" "$LEAPOS_STAGING/boot/grub/grub.cfg"

echo "Staged payload in $LEAPOS_STAGING"
