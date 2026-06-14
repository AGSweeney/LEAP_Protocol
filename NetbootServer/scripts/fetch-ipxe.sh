#!/bin/bash
# Download a prebuilt iPXE legacy BIOS loader into the TFTP staging tree.
# Run on the build host or on the NetBoot server after copying scripts.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="${1:-${LEAP_NETBOOT_ROOT:-/var/lib/leap-netboot}/tftp/ipxe.pxe}"

# boot.ipxe.org serves undionly.kpxe (legacy BIOS). Rename to ipxe.pxe for DHCP option 67.
URL="${IPXE_URL:-https://boot.ipxe.org/undionly.kpxe}"

mkdir -p "$(dirname "$DEST")"
echo "Fetching iPXE loader from $URL ..."
wget -O "$DEST" "$URL"
chmod 644 "$DEST"
ls -lh "$DEST"
echo "Set router DHCP option 67 to: ipxe.pxe"
