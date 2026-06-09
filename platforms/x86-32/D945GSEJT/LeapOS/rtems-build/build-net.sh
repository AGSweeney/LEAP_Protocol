#!/bin/bash
# Full LeapOS networking image: libbsd + net-probe + run-once ISO.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in "$SCRIPT_DIR"/*.sh; do
    sed -i 's/\r$//' "$f" 2>/dev/null || true
done

# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

bash "$SCRIPT_DIR/check-deps.sh"
bash "$SCRIPT_DIR/setup-libbsd.sh"
bash "$SCRIPT_DIR/rebuild-bsp-runonce.sh"

export LEAP_FORCE_RECONFIGURE=1
bash "$SCRIPT_DIR/build-leap-port.sh"
bash "$SCRIPT_DIR/make-boot-image.sh"

echo ""
echo "LEAP discovery ISO (leap-port default, run-once halt):"
ls -lh "$LEAPOS_IMAGE_DIR/leapos-rtems-poc.iso" "$LEAP_PORT_EXE"
