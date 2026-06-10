#!/bin/bash
# Build hybrid GRUB ISO for LeapOS-Gateway only (USB / Etcher DD mode).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/stage-payload.sh" gateway

ISO="$LEAPOS_GATEWAY_ISO"

grub-mkrescue \
    --compress=xz \
    -o "$ISO" \
    "$LEAPOS_GATEWAY_STAGING" \
    2>&1

if [ -f "$LEAP_GATEWAY_EXE" ] && [ "$LEAP_GATEWAY_EXE" != "$LEAPOS_IMAGE_DIR/leap-eip-gateway.exe" ]; then
	cp "$LEAP_GATEWAY_EXE" "$LEAPOS_IMAGE_DIR/leap-eip-gateway.exe"
fi
bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$ISO" "$LEAPOS_IMAGE_DIR/leap-eip-gateway.exe" 2>/dev/null || true
echo "Gateway ISO ready: $ISO"
