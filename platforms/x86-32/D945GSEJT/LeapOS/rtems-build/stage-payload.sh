#!/bin/bash
# Populate rtems-image staging with boot payloads + GRUB config.
# Usage: stage-payload.sh [device|gateway]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

PROFILE="${1:-}"

if [ -z "$PROFILE" ]; then
	echo "Usage: $0 [device|gateway]" >&2
	exit 1
fi

case "$PROFILE" in
device)
	STAGING="$LEAPOS_DEVICE_STAGING"
	GRUB_CFG="$SCRIPT_DIR/grub/leapos-device-grub.cfg"
	;;
gateway)
	STAGING="$LEAPOS_GATEWAY_STAGING"
	GRUB_CFG="$SCRIPT_DIR/grub/leapos-gateway-grub.cfg"
	;;
*)
	echo "Usage: $0 [device|gateway]" >&2
	exit 1
	;;
esac

mkdir -p "$STAGING/boot/grub"

if [ "$PROFILE" = "gateway" ]; then
	if [ ! -f "$LEAP_GATEWAY_EXE" ]; then
		echo "error: leap-eip-gateway.exe not found at $LEAP_GATEWAY_EXE" >&2
		echo "Run: bash rtems-build/build-leap-eip-gateway.sh" >&2
		exit 1
	fi
	cp "$LEAP_GATEWAY_EXE" "$STAGING/leap-eip-gateway.exe"
	echo "Staged leap-eip-gateway.exe (LeapOS-Gateway only)"
else
	if [ ! -f "$LEAP_PORT_EXE" ]; then
		echo "error: leap-port.exe not found at $LEAP_PORT_EXE" >&2
		echo "Run: bash rtems-build/build-leap-port.sh" >&2
		exit 1
	fi
	cp "$LEAP_PORT_EXE" "$STAGING/leap-port.exe"
	echo "Staged leap-port.exe (LeapOS-Device only)"
fi

cp "$GRUB_CFG" "$STAGING/boot/grub/grub.cfg"

echo "Staged $PROFILE payload in $STAGING"
