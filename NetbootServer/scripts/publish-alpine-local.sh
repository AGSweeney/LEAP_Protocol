#!/bin/bash
# Publish Alpine device PXE tree into repo-local NetBoot data (dev/lab).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_ROOT="${LEAP_NETBOOT_ROOT:-$REPO/NetbootServer/local-data}"
PXE_DIR="${1:-$REPO/platforms/x86-32/D945GSEJT/LeapOS/rtems-image/pxe-device-alpine}"

mkdir -p "$DATA_ROOT/state" "$DATA_ROOT/tftp/ipxe/by-mac" "$DATA_ROOT/incoming"

if [ ! -f "$DATA_ROOT/state/settings.json" ]; then
	cp "$REPO/NetbootServer/config/settings.example.json" "$DATA_ROOT/state/settings.json"
fi
if [ ! -f "$DATA_ROOT/state/devices.json" ]; then
	echo '{}' > "$DATA_ROOT/state/devices.json"
fi

export LEAP_NETBOOT_ROOT="$DATA_ROOT"
bash "$SCRIPT_DIR/publish-leapos-device-alpine.sh" \
	"$PXE_DIR" \
	--name "LeapOS device Alpine $(date +%Y-%m-%d)" \
	--default

echo ""
echo "Published to: $DATA_ROOT"
echo "Images:"
cat "$DATA_ROOT/state/images.json"
