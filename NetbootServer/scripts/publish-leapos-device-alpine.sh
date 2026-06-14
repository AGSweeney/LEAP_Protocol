#!/bin/bash
# Publish LeapOS-Device Alpine PXE tree into the NetBoot server's image store.
# Usage:
#   ./publish-leapos-device-alpine.sh /path/to/pxe-device-alpine --name "Alpine device"
#   ./publish-leapos-device-alpine.sh ... --api http://192.168.1.10 --default
set -euo pipefail

API=""
NAME=""
SET_DEFAULT=0
PXE_DIR=""
ALPINE_REPO="${ALPINE_REPO:-http://dl-cdn.alpinelinux.org/alpine/v3.20/main}"
KERNEL_APPEND_BASE="ip=dhcp alpine_repo=${ALPINE_REPO} modules=loop,squashfs,sd-mod,ext4,r8169 earlyprintk=serial,ttyS0,115200 console=tty1 console=ttyS0,115200n8"
VMLINUZ="${VMLINUZ:-vmlinuz-lts}"
INITRAMFS="${INITRAMFS:-initramfs-lts}"
MODLOOP="${MODLOOP:-modloop-lts}"
APKOVL="${APKOVL:-leap-device.apkovl.tar.gz}"

usage() {
	echo "Usage: $0 <pxe-device-alpine-dir> [--name LABEL] [--api URL] [--default]" >&2
	exit 1
}

while [ $# -gt 0 ]; do
	case "$1" in
	--name) NAME="$2"; shift 2 ;;
	--api) API="$2"; shift 2 ;;
	--default) SET_DEFAULT=1; shift ;;
	-h|--help) usage ;;
	-*)
		echo "unknown option: $1" >&2
		usage
		;;
	*)
		if [ -z "$PXE_DIR" ]; then
			PXE_DIR="$1"
			shift
		else
			echo "unexpected argument: $1" >&2
			usage
		fi
		;;
	esac
done

[ -n "$PXE_DIR" ] && [ -d "$PXE_DIR" ] || usage
[ -f "$PXE_DIR/$VMLINUZ" ] || { echo "error: missing $PXE_DIR/$VMLINUZ" >&2; exit 1; }
[ -f "$PXE_DIR/$INITRAMFS" ] || { echo "error: missing $PXE_DIR/$INITRAMFS" >&2; exit 1; }
[ -f "$PXE_DIR/$MODLOOP" ] || { echo "error: missing $PXE_DIR/$MODLOOP" >&2; exit 1; }
[ -f "$PXE_DIR/$APKOVL" ] || { echo "error: missing $PXE_DIR/$APKOVL" >&2; exit 1; }
[ -n "$NAME" ] || NAME="LeapOS device Alpine $(date +%Y-%m-%d)"

if [ -n "$API" ]; then
	BUNDLE="$(mktemp "${TMPDIR:-/tmp}/leap-alpine-pxe.XXXXXX.tar.gz")"
	trap 'rm -f "$BUNDLE"' EXIT
	tar -czf "$BUNDLE" -C "$PXE_DIR" "$VMLINUZ" "$INITRAMFS" "$MODLOOP" "$APKOVL"
	CURL_ARGS=(
		-fsS -X POST "${API%/}/api/v1/images"
		-F "type=alpine-linux"
		-F "name=${NAME}"
		-F "kernel_args=${KERNEL_APPEND_BASE}"
		-F "file=@${BUNDLE}"
	)
	if [ "$SET_DEFAULT" = "1" ]; then
		CURL_ARGS+=(-F "set_default=1")
	fi
	echo "Uploading Alpine image to ${API%/}/api/v1/images ..."
	curl "${CURL_ARGS[@]}"
	echo
	exit 0
fi

DATA_ROOT="${LEAP_NETBOOT_ROOT:-/var/lib/leap-netboot}"
IMAGE_ID="$(uuidgen 2>/dev/null | tr -d '-' | cut -c1-12 || python3 -c 'import uuid; print(uuid.uuid4().hex[:12])')"
DEST_DIR="$DATA_ROOT/tftp/images/$IMAGE_ID"
mkdir -p "$DEST_DIR"

cp -- "$PXE_DIR/$VMLINUZ" "$DEST_DIR/"
cp -- "$PXE_DIR/$INITRAMFS" "$DEST_DIR/"
cp -- "$PXE_DIR/$MODLOOP" "$DEST_DIR/"
cp -- "$PXE_DIR/$APKOVL" "$DEST_DIR/"

SIZE="$((
	$(wc -c < "$DEST_DIR/$VMLINUZ" | tr -d ' ') +
	$(wc -c < "$DEST_DIR/$INITRAMFS" | tr -d ' ') +
	$(wc -c < "$DEST_DIR/$MODLOOP" | tr -d ' ') +
	$(wc -c < "$DEST_DIR/$APKOVL" | tr -d ' ')
))"
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
    "type": "alpine-linux",
    "filename": "${MODLOOP}",
    "vmlinuz": "${VMLINUZ}",
    "initramfs": "${INITRAMFS}",
    "modloop": "${MODLOOP}",
    "apkovl": "${APKOVL}",
    "alpine_repo": """${ALPINE_REPO}""",
    "kernel_args": """${KERNEL_APPEND_BASE}""",
    "size_bytes": int("${SIZE}"),
    "created": "${CREATED}",
}
images_file.parent.mkdir(parents=True, exist_ok=True)
images_file.write_text(json.dumps(images, indent=2) + "\n")
if ${SET_DEFAULT}:
    settings = json.loads(settings_file.read_text()) if settings_file.is_file() else {}
    settings["default_image_id"] = "${IMAGE_ID}"
    settings_file.write_text(json.dumps(settings, indent=2) + "\n")
print("Published Alpine image ${IMAGE_ID} -> ${DEST_DIR}/")
PY

/opt/leap-netboot/scripts/render-pxe.sh 2>/dev/null || \
	"$(dirname "$0")/render-pxe.sh"
