#!/bin/bash
# Populate rtems-image staging with LeapPort device boot payload + GRUB config.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

STAGING="$LEAPOS_DEVICE_STAGING"
GRUB_CFG="$SCRIPT_DIR/grub/leapos-device-grub.cfg"

mkdir -p "$STAGING/boot/grub"

if [ ! -f "$LEAP_PORT_EXE" ]; then
	echo "error: leap-port.exe not found at $LEAP_PORT_EXE" >&2
	echo "Run: bash rtems-build/build-leap-port.sh" >&2
	exit 1
fi

cp "$LEAP_PORT_EXE" "$STAGING/leap-port.exe"
cp "$GRUB_CFG" "$STAGING/boot/grub/grub.cfg"

echo "Staged leap-port.exe (LeapOS-Device) in $STAGING"
