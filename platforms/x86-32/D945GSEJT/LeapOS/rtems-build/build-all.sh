#!/bin/bash
# Full LeapOS boot-image pipeline (net-probe + ISO + CF raw image).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Normalize CRLF when editing scripts on Windows.
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
    leap-port)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        echo "leap-port.exe only — done"
        ;;
    iso)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/make-boot-image.sh"
        ;;
    cf)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        if [ "$(id -u)" -eq 0 ]; then
            bash "$SCRIPT_DIR/make-cf-image.sh"
        else
            echo "Building CF image (requires sudo for loop mount)..."
            sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh"
        fi
        ;;
    all)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/build-net-probe.sh"
        bash "$SCRIPT_DIR/make-boot-image.sh"
        if [ "$(id -u)" -eq 0 ]; then
            bash "$SCRIPT_DIR/make-cf-image.sh"
        else
            echo "Building CF image (requires sudo for loop mount)..."
            sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh"
        fi
        echo ""
        echo "LeapOS boot artifacts:"
        ls -lh "$LEAPOS_IMAGE_DIR"/leapos-rtems-poc.iso \
               "$LEAPOS_IMAGE_DIR"/leapos-rtems-poc.img \
               "$LEAPOS_IMAGE_DIR"/leap-port.exe \
               "$LEAPOS_IMAGE_DIR"/net-probe.exe 2>/dev/null || true
        ;;
    *)
        echo "Usage: $0 [all|leap-port|net-probe|iso|cf]" >&2
        exit 1
        ;;
esac

echo "Done. Flash instructions: $LEAPOS_IMAGE_DIR/README.txt"
