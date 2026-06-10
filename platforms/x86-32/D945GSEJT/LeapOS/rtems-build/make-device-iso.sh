#!/bin/bash
# Build hybrid GRUB ISO for LeapOS-Device only (USB / Etcher DD mode).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/stage-payload.sh" device

grub-mkrescue \
    --compress=xz \
    -o "$LEAPOS_DEVICE_ISO" \
    "$LEAPOS_DEVICE_STAGING" \
    2>&1

if [ "$LEAP_PORT_EXE" != "$LEAPOS_IMAGE_DIR/leap-port.exe" ]; then
	cp "$LEAP_PORT_EXE" "$LEAPOS_IMAGE_DIR/leap-port.exe"
fi
bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$LEAPOS_DEVICE_ISO" "$LEAPOS_IMAGE_DIR/leap-port.exe" 2>/dev/null || true
echo "Device ISO ready: $LEAPOS_DEVICE_ISO"
