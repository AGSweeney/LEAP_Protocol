#!/bin/bash
# Rebuild boot ISO with D945GSEJT VGA text-console GRUB args (no BSP rebuild).
#
# Intel 945GSE IGP fails RTEMS VBE init.  Boot with --video=off and vgacons.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in "$SCRIPT_DIR"/*.sh; do
    sed -i 's/\r$//' "$f" 2>/dev/null || true
done

# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/check-deps.sh"
bash "$SCRIPT_DIR/make-device-iso.sh"

echo ""
echo "VGA LeapOS-Device ISO (boot with --video=off --console=/dev/vgacons):"
ls -lh "$LEAPOS_DEVICE_ISO"
