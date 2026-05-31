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
STARTUP_S="${LEAP_WIRE_STARTUP_S:-2}"
TIMEOUT_S="${LEAP_WIRE_TIMEOUT_S:-45}"
WAIT_ITERATIONS="${LEAP_WIRE_WAIT_ITER:-30}"

DEVICE="$BUILD/leap_linux_device"
CTRL="$BUILD/leap_linux_controller"

if sudo -n true 2>/dev/null; then
    SUDO=(sudo -n)
else
    SUDO=(sudo)
fi

wire_smoke_skip() {
    echo "wire smoke: skipped — $*"
    exit 0
}

wire_smoke_fail() {
    echo "wire smoke: FAILED — $*" >&2
    exit 1
}

wire_smoke_is_wsl2() {
    grep -qiE 'microsoft|wsl' /proc/version 2>/dev/null
}

wire_smoke_preflight() {
    if [[ "${LEAP_SKIP_WIRE_SMOKE:-0}" == "1" ]]; then
        wire_smoke_skip "LEAP_SKIP_WIRE_SMOKE=1"
    fi

    if wire_smoke_is_wsl2; then
        wire_smoke_skip "WSL2 cannot bind AF_PACKET (use native Linux or CI)"
    fi

    if [[ ! -x "$DEVICE" || ! -x "$CTRL" ]]; then
        wire_smoke_fail "missing executables under $BUILD"
    fi

    if ! command -v ip >/dev/null 2>&1; then
        wire_smoke_fail "ip(8) not found (install iproute2)"
    fi

    if ! ip link show "$IFACE" >/dev/null 2>&1; then
        wire_smoke_fail "interface '$IFACE' not found"
    fi

    "${SUDO[@]}" ip link set "$IFACE" up >/dev/null 2>&1 || true
}

wire_smoke_wait_for_device() {
    local dev_log="$1"
    local dev_pid="$2"
    local i

    for ((i = 0; i < WAIT_ITERATIONS; i++)); do
        if grep -q "LEAP device on" "$dev_log" 2>/dev/null; then
            return 0
        fi

        if ! kill -0 "$dev_pid" 2>/dev/null; then
            echo "wire smoke: device process exited during startup:" >&2
            cat "$dev_log" >&2
            return 1
        fi

        sleep 0.2
    done

    echo "wire smoke: timed out waiting for device (log follows):" >&2
    cat "$dev_log" >&2
    return 1
}

wire_smoke_preflight

echo "wire smoke: device on $IFACE (build=$BUILD)"

DEV_LOG="$(mktemp)"

cleanup() {
    "${SUDO[@]}" kill "$DEV_PID" 2>/dev/null || true
    wait "$DEV_PID" 2>/dev/null || true
    rm -f "$DEV_LOG"
}
trap cleanup EXIT

"${SUDO[@]}" "$DEVICE" "$IFACE" >"$DEV_LOG" 2>&1 &
DEV_PID=$!

if ! wire_smoke_wait_for_device "$DEV_LOG" "$DEV_PID"; then
    if [[ "${CI:-}" == "true" && "${LEAP_WIRE_SMOKE_REQUIRED:-0}" != "1" ]]; then
        wire_smoke_skip "device could not bind AF_PACKET on $IFACE (see log above)"
    fi
    wire_smoke_fail "device startup failed on $IFACE"
fi

echo "wire smoke: controller bootstrap + single PD write"
if ! timeout "$TIMEOUT_S" "${SUDO[@]}" "$CTRL" "$IFACE"; then
    echo "wire smoke: controller log (device still running):" >&2
    tail -n 20 "$DEV_LOG" >&2 || true
    wire_smoke_fail "controller did not complete within ${TIMEOUT_S}s"
fi

echo "wire smoke: OK"
