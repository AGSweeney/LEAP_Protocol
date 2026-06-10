#!/bin/bash
# LeapOS boot-image pipeline — separate Device and Gateway products only.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in "$SCRIPT_DIR"/*.sh; do
    sed -i 's/\r$//' "$f" 2>/dev/null || true
done

# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

MODE="${1:-all}"

bash "$SCRIPT_DIR/check-deps.sh"

build_cf_images() {
	if [ "$(id -u)" -eq 0 ]; then
		bash "$SCRIPT_DIR/make-cf-image.sh" device
		bash "$SCRIPT_DIR/make-cf-image.sh" gateway
	else
		echo "Building CF images (requires sudo for loop mount)..."
		sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh" device
		sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh" gateway
	fi
}

case "$MODE" in
    net-probe)
        bash "$SCRIPT_DIR/build-net-probe.sh"
        echo "net-probe.exe only — done"
        ;;
    device|leap-port)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        echo "LeapOS-Device (leap-port.exe) — done"
        ;;
    gateway)
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        echo "LeapOS-Gateway (leap-eip-gateway.exe) — done"
        ;;
    iso)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        bash "$SCRIPT_DIR/make-device-iso.sh"
        bash "$SCRIPT_DIR/make-gateway-iso.sh"
        ;;
    iso-device|device-iso)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/make-device-iso.sh"
        ;;
    iso-gateway|gateway-iso)
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        bash "$SCRIPT_DIR/make-gateway-iso.sh"
        ;;
    cf)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        build_cf_images
        ;;
    cf-device)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        if [ "$(id -u)" -eq 0 ]; then
            bash "$SCRIPT_DIR/make-cf-image.sh" device
        else
            sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh" device
        fi
        ;;
    cf-gateway)
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        if [ "$(id -u)" -eq 0 ]; then
            bash "$SCRIPT_DIR/make-cf-image.sh" gateway
        else
            sudo IMAGE_MB="${IMAGE_MB:-128}" bash "$SCRIPT_DIR/make-cf-image.sh" gateway
        fi
        ;;
    all)
        bash "$SCRIPT_DIR/build-leap-port.sh"
        bash "$SCRIPT_DIR/build-leap-eip-gateway.sh"
        bash "$SCRIPT_DIR/build-net-probe.sh"
        bash "$SCRIPT_DIR/make-device-iso.sh"
        bash "$SCRIPT_DIR/make-gateway-iso.sh"
        build_cf_images
        echo ""
        echo "LeapOS boot artifacts:"
        ls -lh "$LEAPOS_IMAGE_DIR"/leapos-device.iso \
               "$LEAPOS_IMAGE_DIR"/leapos-gateway.iso \
               "$LEAPOS_IMAGE_DIR"/leapos-device.img \
               "$LEAPOS_IMAGE_DIR"/leapos-gateway.img \
               "$LEAPOS_IMAGE_DIR"/leap-port.exe \
               "$LEAPOS_IMAGE_DIR"/leap-eip-gateway.exe \
               "$LEAPOS_IMAGE_DIR"/net-probe.exe 2>/dev/null || true
        ;;
    *)
        echo "Usage: $0 [all|device|gateway|net-probe|iso|iso-device|iso-gateway|cf|cf-device|cf-gateway]" >&2
        echo "  iso-device  = leapos-device.iso only" >&2
        echo "  iso-gateway = leapos-gateway.iso only" >&2
        exit 1
        ;;
esac

echo "Done. Flash instructions: $LEAPOS_IMAGE_DIR/README.txt"
