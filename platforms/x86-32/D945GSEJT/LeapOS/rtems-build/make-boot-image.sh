#!/bin/bash
# Build hybrid GRUB ISO for USB / CF (via Etcher DD mode).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/stage-payload.sh"

ISO="$LEAPOS_IMAGE_DIR/leapos-rtems-poc.iso"

grub-mkrescue \
    --compress=xz \
    -o "$ISO" \
    "$LEAPOS_STAGING" \
    2>&1

if [ "$LEAP_PORT_EXE" != "$LEAPOS_IMAGE_DIR/leap-port.exe" ]; then
	cp "$LEAP_PORT_EXE" "$LEAPOS_IMAGE_DIR/leap-port.exe"
fi
if [ -f "$NET_PROBE_EXE" ] && [ "$NET_PROBE_EXE" != "$LEAPOS_IMAGE_DIR/net-probe.exe" ]; then
	cp "$NET_PROBE_EXE" "$LEAPOS_IMAGE_DIR/net-probe.exe"
fi
bash "$SCRIPT_DIR/write-image-readme.sh"

ls -lh "$ISO" "$LEAPOS_IMAGE_DIR/leap-port.exe" "$LEAPOS_IMAGE_DIR/net-probe.exe" 2>/dev/null || true
echo "ISO ready: $ISO"
