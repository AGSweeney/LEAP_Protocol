#!/usr/bin/env bash
#
# End-to-end LEAP smoke test (requires CAP_NET_RAW / sudo).
#
# On CI and other hosts where AF_PACKET bind on lo returns ENODEV, uses an
# isolated veth pair on a bridge instead of loopback.
#
# Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${LEAP_BUILD_DIR:-$ROOT/build}"
IFACE="${LEAP_IFACE:-lo}"
TIMEOUT_S="${LEAP_WIRE_TIMEOUT_S:-45}"
WAIT_ITERATIONS="${LEAP_WIRE_WAIT_ITER:-30}"

DEVICE="$BUILD/leap_linux_device"
CTRL="$BUILD/leap_linux_controller"

# shellcheck source=tools/ci/wire_smoke_common.sh
source "$(dirname "${BASH_SOURCE[0]}")/wire_smoke_common.sh"

wire_smoke_common_init_sudo
SUDO=("${WIRE_SMOKE_SUDO[@]}")

wire_smoke_skip() {
    echo "wire smoke: skipped — $*"
    exit 0
}

wire_smoke_fail() {
    echo "wire smoke: FAILED — $*" >&2
    exit 1
}

wire_smoke_preflight() {
    if [[ "${LEAP_SKIP_WIRE_SMOKE:-0}" == "1" ]]; then
        wire_smoke_skip "LEAP_SKIP_WIRE_SMOKE=1"
    fi

    if wire_smoke_common_is_wsl2; then
        wire_smoke_skip "WSL2 cannot bind AF_PACKET (use native Linux or CI)"
    fi

    if [[ ! -x "$DEVICE" || ! -x "$CTRL" ]]; then
        wire_smoke_fail "missing executables under $BUILD"
    fi

    if ! command -v ip >/dev/null 2>&1; then
        wire_smoke_fail "ip(8) not found (install iproute2)"
    fi
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

DEV_IFACE="$IFACE"
CTRL_IFACE="$IFACE"

if wire_smoke_common_need_veth; then
    echo "wire smoke: using veth bridge (AF_PACKET on lo unavailable in this environment)"
    wire_smoke_common_veth_setup \
        "${LEAP_SMOKE_BRIDGE:-br-leap-lo-smoke}" \
        "${LEAP_SMOKE_VETH_A:-veth-leap-lo0}" \
        "${LEAP_SMOKE_VETH_B:-veth-leap-lo1}"
    DEV_IFACE="$WIRE_SMOKE_DEV_IF"
    CTRL_IFACE="$WIRE_SMOKE_CTRL_IF"
else
    if ! ip link show "$IFACE" >/dev/null 2>&1; then
        wire_smoke_fail "interface '$IFACE' not found"
    fi
    "${SUDO[@]}" ip link set "$IFACE" up >/dev/null 2>&1 || true
fi

echo "wire smoke: device on $DEV_IFACE, controller on $CTRL_IFACE (build=$BUILD)"

DEV_LOG="$(mktemp)"

cleanup() {
    "${SUDO[@]}" kill "$DEV_PID" 2>/dev/null || true
    wait "$DEV_PID" 2>/dev/null || true
    wire_smoke_common_veth_teardown
    rm -f "$DEV_LOG"
}
trap cleanup EXIT

"${SUDO[@]}" "$DEVICE" "$DEV_IFACE" >"$DEV_LOG" 2>&1 &
DEV_PID=$!

if ! wire_smoke_wait_for_device "$DEV_LOG" "$DEV_PID"; then
    if [[ "${CI:-}" == "true" && "${LEAP_WIRE_SMOKE_REQUIRED:-0}" != "1" ]]; then
        wire_smoke_skip "device could not bind AF_PACKET (see log above)"
    fi
    wire_smoke_fail "device startup failed on $DEV_IFACE"
fi

echo "wire smoke: controller bootstrap + single PD write"
if ! timeout "$TIMEOUT_S" "${SUDO[@]}" "$CTRL" "$CTRL_IFACE"; then
    echo "wire smoke: controller log (device still running):" >&2
    tail -n 20 "$DEV_LOG" >&2 || true
    wire_smoke_fail "controller did not complete within ${TIMEOUT_S}s"
fi

echo "wire smoke: OK"
