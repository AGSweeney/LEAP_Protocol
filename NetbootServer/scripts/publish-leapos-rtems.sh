#!/bin/bash
# Publish leap-port.exe (or copy) into the NetBoot server's image store.
# Usage on server:
#   ./publish-leapos-rtems.sh /path/to/leap-port.exe --name "build 2026-06-12"
#
# Or from WSL against a running server's API:
#   ./publish-leapos-rtems.sh leap-port.exe --api http://192.168.1.10 --name "nightly"
set -euo pipefail

API=""
NAME=""
SET_DEFAULT=0
KERNEL_ARGS="--video=off --console=/dev/com1,115200 --printk=/dev/com1,115200"
PAYLOAD=""

usage() {
	echo "Usage: $0 <leap-port.exe> [--name LABEL] [--api URL] [--default] [--kernel-args ARGS]" >&2
	exit 1
}

while [ $# -gt 0 ]; do
	case "$1" in
	--name) NAME="$2"; shift 2 ;;
	--api) API="$2"; shift 2 ;;
	--default) SET_DEFAULT=1; shift ;;
	--kernel-args) KERNEL_ARGS="$2"; shift 2 ;;
	-h|--help) usage ;;
	-*)
		echo "unknown option: $1" >&2
		usage
		;;
	*)
		if [ -z "$PAYLOAD" ]; then
			PAYLOAD="$1"
			shift
		else
			echo "unexpected argument: $1" >&2
			usage
		fi
		;;
	esac
done

[ -n "$PAYLOAD" ] && [ -f "$PAYLOAD" ] || usage
[ -n "$NAME" ] || NAME="$(basename "$PAYLOAD") $(date +%Y-%m-%d)"

if [ -n "$API" ]; then
	curl -fsS -X POST "$API/api/v1/images" \
		-F "file=@${PAYLOAD};filename=leap-port.exe" \
		-F "name=${NAME}" \
		-F "type=rtems-multiboot" \
		-F "kernel_args=${KERNEL_ARGS}" \
		-F "set_default=${SET_DEFAULT}"
	echo
	exit 0
fi

# Local publish into /var/lib/leap-netboot (run on server as root).
DATA_ROOT="${LEAP_NETBOOT_ROOT:-/var/lib/leap-netboot}"
IMAGE_ID="$(uuidgen 2>/dev/null | tr -d '-' | cut -c1-12 || python3 -c 'import uuid; print(uuid.uuid4().hex[:12])')"
DEST_DIR="$DATA_ROOT/tftp/images/$IMAGE_ID"
mkdir -p "$DEST_DIR"
cp -- "$PAYLOAD" "$DEST_DIR/leap-port.exe"
SIZE="$(wc -c < "$DEST_DIR/leap-port.exe" | tr -d ' ')"
CREATED="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

python3 <<PY
import json
from pathlib import Path

data_root = Path("${DATA_ROOT}")
images_file = data_root / "state/images.json"
settings_file = data_root / "state/settings.json"
images = json.loads(images_file.read_text()) if images_file.is_file() else {}
images["${IMAGE_ID}"] = {
    "id": "${IMAGE_ID}",
    "name": """${NAME}""",
    "type": "rtems-multiboot",
    "filename": "leap-port.exe",
    "kernel_args": """${KERNEL_ARGS}""",
    "size_bytes": int("${SIZE}"),
    "created": "${CREATED}",
}
images_file.parent.mkdir(parents=True, exist_ok=True)
images_file.write_text(json.dumps(images, indent=2) + "\n")
if ${SET_DEFAULT}:
    settings = json.loads(settings_file.read_text()) if settings_file.is_file() else {}
    settings["default_image_id"] = "${IMAGE_ID}"
    settings_file.write_text(json.dumps(settings, indent=2) + "\n")
print("Published image ${IMAGE_ID} -> ${DEST_DIR}/leap-port.exe")
PY

/opt/leap-netboot/scripts/render-pxe.sh 2>/dev/null || \
	"$(dirname "$0")/render-pxe.sh"
