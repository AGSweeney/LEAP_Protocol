#!/bin/bash
# Build run-once LeapOS image: net-probe halts after exit (no reboot loop).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in "$SCRIPT_DIR"/*.sh; do
    sed -i 's/\r$//' "$f" 2>/dev/null || true
done

# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/check-deps.sh"
bash "$SCRIPT_DIR/rebuild-bsp-runonce.sh"

export LEAP_FORCE_RECONFIGURE=1
bash "$SCRIPT_DIR/build-net-probe.sh"
bash "$SCRIPT_DIR/make-boot-image.sh"

echo ""
echo "Run-once ISO (net-probe exits and board halts — no GRUB reboot loop):"
ls -lh "$LEAPOS_IMAGE_DIR/leapos-rtems-poc.iso"
