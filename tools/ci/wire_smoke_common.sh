# wire_smoke_common.sh — shared network setup for LEAP CI smoke tests.
#
# Sourced by wire_smoke_lo.sh and wire_smoke_discover_lo.sh (do not execute directly).
#
# Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
# SPDX-License-Identifier: MIT

wire_smoke_common_init_sudo() {
    if sudo -n true 2>/dev/null; then
        WIRE_SMOKE_SUDO=(sudo -n)
    else
        WIRE_SMOKE_SUDO=(sudo)
    fi
}

wire_smoke_common_is_wsl2() {
    grep -qiE 'microsoft|wsl' /proc/version 2>/dev/null
}

# GitHub-hosted runners expose lo but often reject AF_PACKET bind (ENODEV).
wire_smoke_common_need_veth() {
    if [[ "${LEAP_FORCE_VETH:-0}" == "1" ]]; then
        return 0
    fi

    if [[ "${CI:-}" == "true" ]]; then
        return 0
    fi

    return 1
}

wire_smoke_common_veth_setup() {
    local bridge="$1"
    local veth_a="$2"
    local veth_b="$3"
    local mac_a="${4:-02:bb:00:00:00:01}"
    local mac_b="${5:-02:bb:00:00:00:02}"

    WIRE_SMOKE_VETH_ACTIVE=1
    WIRE_SMOKE_VETH_BRIDGE="$bridge"
    WIRE_SMOKE_VETH_A="$veth_a"

    "${WIRE_SMOKE_SUDO[@]}" ip link del "$veth_a" 2>/dev/null || true
    "${WIRE_SMOKE_SUDO[@]}" ip link del "$bridge" 2>/dev/null || true

    "${WIRE_SMOKE_SUDO[@]}" ip link add "$veth_a" type veth peer name "$veth_b"
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_a" address "$mac_a"
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_b" address "$mac_b"
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_a" up
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_b" up

    "${WIRE_SMOKE_SUDO[@]}" ip link add "$bridge" type bridge
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_a" master "$bridge"
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$veth_b" master "$bridge"
    "${WIRE_SMOKE_SUDO[@]}" ip link set "$bridge" up

    WIRE_SMOKE_DEV_IF="$veth_a"
    WIRE_SMOKE_CTRL_IF="$veth_b"
}

wire_smoke_common_veth_teardown() {
    if [[ "${WIRE_SMOKE_VETH_ACTIVE:-0}" != "1" ]]; then
        return 0
    fi

    if [[ -n "${WIRE_SMOKE_VETH_A:-}" ]]; then
        "${WIRE_SMOKE_SUDO[@]}" ip link del "$WIRE_SMOKE_VETH_A" 2>/dev/null || true
    fi

    if [[ -n "${WIRE_SMOKE_VETH_BRIDGE:-}" ]]; then
        "${WIRE_SMOKE_SUDO[@]}" ip link del "$WIRE_SMOKE_VETH_BRIDGE" 2>/dev/null || true
    fi

    WIRE_SMOKE_VETH_ACTIVE=0
}
