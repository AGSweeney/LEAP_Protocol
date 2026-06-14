#!/bin/bash
# LeapOS boot-image pipeline — LeapPort device only.
# Gateway product: LeapGateway-linux/ (Alpine i386 Linux).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in "$SCRIPT_DIR"/*.sh; do
    sed -i 's/\r$//' "$f" 2>/dev/null || true
done

# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

MODE="${1:-all}"

bash "$SCRIPT_DIR/check-deps.sh"

case "$MODE" in
    net-probe)
        bash "$SCRIPT_DIR/build-net-probe.sh"
        echo "net-probe.exe only — done"
        ;;
    device|leap-port)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        echo "LeapOS-Device (leap-port.exe) — done"
        ;;
    iso|iso-device|device-iso)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/make-device-iso.sh"
        ;;
    cf|cf-device)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/make-cf-image.sh"
        ;;
    all)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/build-net-probe.sh"
        bash "$SCRIPT_DIR/make-device-iso.sh"
        bash "$SCRIPT_DIR/make-cf-image.sh"
        echo ""
        echo "LeapOS boot artifacts:"
        ls -lh "$LEAPOS_IMAGE_DIR"/leapos-device.iso \
               "$LEAPOS_IMAGE_DIR"/leapos-device.img \
               "$LEAPOS_IMAGE_DIR"/leap-port.exe \
               "$LEAPOS_IMAGE_DIR"/net-probe.exe 2>/dev/null || true
        ;;
    *)
        echo "Usage: $0 [all|device|net-probe|iso|iso-device|cf|cf-device]" >&2
        echo "  Gateway images: see LeapGateway-linux/ (Alpine Linux)" >&2
        exit 1
        ;;
esac

echo "Done. Flash instructions: $LEAPOS_IMAGE_DIR/README.txt"
