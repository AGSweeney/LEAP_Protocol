#!/usr/bin/env bash
#
# Multi-device LEAP discovery smoke test (requires CAP_NET_RAW / sudo).
#
# Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${LEAP_BUILD_DIR:-$ROOT/build}"
SCAN_MS="${LEAP_DISCOVER_SCAN_MS:-3000}"
TIMEOUT_S="${LEAP_WIRE_TIMEOUT_S:-60}"
WAIT_ITERATIONS="${LEAP_WIRE_WAIT_ITER:-30}"

BRIDGE="${LEAP_DISCOVER_BRIDGE:-br-leap-smoke}"
VETH_A="${LEAP_DISCOVER_VETH_A:-veth-leap0}"
VETH_B="${LEAP_DISCOVER_VETH_B:-veth-leap1}"
MAC_A="${LEAP_DISCOVER_MAC_A:-02:aa:00:00:00:01}"
MAC_B="${LEAP_DISCOVER_MAC_B:-02:aa:00:00:00:02}"

DEVICE="$BUILD/leap_linux_device"
DISCOVER="$BUILD/leap_linux_discover"

# shellcheck source=tools/ci/wire_smoke_common.sh
source "$(dirname "${BASH_SOURCE[0]}")/wire_smoke_common.sh"

wire_smoke_common_init_sudo
SUDO=("${WIRE_SMOKE_SUDO[@]}")

wire_smoke_skip() {
    echo "discover smoke: skipped — $*"
    exit 0
}

wire_smoke_fail() {
    echo "discover smoke: FAILED — $*" >&2
    exit 1
}

wire_smoke_preflight() {
    if [[ "${LEAP_SKIP_WIRE_SMOKE:-0}" == "1" ]]; then
        wire_smoke_skip "LEAP_SKIP_WIRE_SMOKE=1"
    fi

    if wire_smoke_common_is_wsl2; then
        wire_smoke_skip "WSL2 cannot bind AF_PACKET (use native Linux or CI)"
    fi

    if [[ ! -x "$DEVICE" || ! -x "$DISCOVER" ]]; then
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
            echo "discover smoke: device process exited during startup:" >&2
            cat "$dev_log" >&2
            return 1
        fi

        sleep 0.2
    done

    echo "discover smoke: timed out waiting for device (log follows):" >&2
    cat "$dev_log" >&2
    return 1
}

wire_smoke_preflight

wire_smoke_common_veth_setup "$BRIDGE" "$VETH_A" "$VETH_B" "$MAC_A" "$MAC_B"

DEV_LOG_A="$(mktemp)"
DEV_LOG_B="$(mktemp)"
DISC_LOG="$(mktemp)"

cleanup() {
    "${SUDO[@]}" kill "$DEV_PID_A" "$DEV_PID_B" 2>/dev/null || true
    wait "$DEV_PID_A" 2>/dev/null || true
    wait "$DEV_PID_B" 2>/dev/null || true
    wire_smoke_common_veth_teardown
    rm -f "$DEV_LOG_A" "$DEV_LOG_B" "$DISC_LOG"
}
trap cleanup EXIT

echo "discover smoke: two devices on $VETH_A / $VETH_B, scan on $BRIDGE"

"${SUDO[@]}" "$DEVICE" "$VETH_A" >"$DEV_LOG_A" 2>&1 &
DEV_PID_A=$!
"${SUDO[@]}" "$DEVICE" "$VETH_B" >"$DEV_LOG_B" 2>&1 &
DEV_PID_B=$!

if ! wire_smoke_wait_for_device "$DEV_LOG_A" "$DEV_PID_A"; then
    wire_smoke_fail "device A startup failed"
fi
if ! wire_smoke_wait_for_device "$DEV_LOG_B" "$DEV_PID_B"; then
    wire_smoke_fail "device B startup failed"
fi

sleep 0.5

echo "discover smoke: scanning for peers (${SCAN_MS} ms)"
set +e
timeout "$TIMEOUT_S" "${SUDO[@]}" "$DISCOVER" --scan-ms "$SCAN_MS" "$BRIDGE" >"$DISC_LOG" 2>&1
disc_rc=$?
set -e

cat "$DISC_LOG"

if [[ "$disc_rc" -ne 0 ]]; then
    echo "discover smoke: device A log:" >&2
    tail -n 15 "$DEV_LOG_A" >&2 || true
    echo "discover smoke: device B log:" >&2
    tail -n 15 "$DEV_LOG_B" >&2 || true
    wire_smoke_fail "discover exited with status $disc_rc"
fi

peer_count="$(grep -c '^  peer ' "$DISC_LOG" || true)"
if [[ "$peer_count" -lt 2 ]]; then
    wire_smoke_fail "expected >= 2 peers, found $peer_count"
fi

echo "discover smoke: OK ($peer_count peers)"
