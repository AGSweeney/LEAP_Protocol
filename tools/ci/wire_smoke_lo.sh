#!/usr/bin/env bash
#
# End-to-end LEAP smoke test on loopback (requires CAP_NET_RAW / sudo).
#
# Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${LEAP_BUILD_DIR:-$ROOT/build}"
IFACE="${LEAP_IFACE:-lo}"
STARTUP_S="${LEAP_WIRE_STARTUP_S:-1}"
TIMEOUT_S="${LEAP_WIRE_TIMEOUT_S:-45}"

DEVICE="$BUILD/leap_linux_device"
CTRL="$BUILD/leap_linux_controller"

if [[ ! -x "$DEVICE" || ! -x "$CTRL" ]]; then
    echo "wire smoke: missing executables under $BUILD" >&2
    exit 1
fi

echo "wire smoke: device on $IFACE (build=$BUILD)"

sudo "$DEVICE" "$IFACE" &
DEV_PID=$!

cleanup() {
    sudo kill "$DEV_PID" 2>/dev/null || true
    wait "$DEV_PID" 2>/dev/null || true
}
trap cleanup EXIT

sleep "$STARTUP_S"

echo "wire smoke: controller bootstrap + single PD write"
timeout "$TIMEOUT_S" sudo "$CTRL" "$IFACE"

echo "wire smoke: OK"
