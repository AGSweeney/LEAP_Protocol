#!/bin/sh
# Regenerate GRUB and iPXE menus from /var/lib/leap-netboot/state/*.json
set -eu

DATA_ROOT="${LEAP_NETBOOT_ROOT:-/var/lib/leap-netboot}"
STATE_DIR="$DATA_ROOT/state"
TFTP_ROOT="$DATA_ROOT/tftp"
IMAGES_DIR="$TFTP_ROOT/images"
IPXE_DIR="$TFTP_ROOT/ipxe"
BY_MAC_DIR="$IPXE_DIR/by-mac"
GRUB_DIR="$TFTP_ROOT/boot/grub"

IMAGES_JSON="$STATE_DIR/images.json"
DEVICES_JSON="$STATE_DIR/devices.json"
SETTINGS_JSON="$STATE_DIR/settings.json"

mkdir -p "$GRUB_DIR" "$IPXE_DIR" "$BY_MAC_DIR" "$IMAGES_DIR"

python3 <<'PY'
import json
import os
import re
from pathlib import Path

DATA_ROOT = Path(os.environ.get("LEAP_NETBOOT_ROOT", "/var/lib/leap-netboot"))
STATE_DIR = DATA_ROOT / "state"
TFTP_ROOT = DATA_ROOT / "tftp"
GRUB_DIR = TFTP_ROOT / "boot/grub"
IPXE_DIR = TFTP_ROOT / "ipxe"
BY_MAC_DIR = IPXE_DIR / "by-mac"

DEFAULT_RTEMS_ARGS = (
    "--video=off --console=/dev/com1,115200 --printk=/dev/com1,115200"
)
DEFAULT_ALPINE_REPO = "http://dl-cdn.alpinelinux.org/alpine/v3.20/main"
DEFAULT_ALPINE_APPEND = (
    "ip=dhcp modules=loop,squashfs,sd-mod,ext4,r8169 earlyprintk=serial,ttyS0,115200 "
    "console=tty1 console=ttyS0,115200n8"
)


def load(path, default):
    if not path.is_file():
        return default
    with path.open() as fh:
        return json.load(fh)


def http_boot_host(settings: dict) -> str:
    for key in ("http_boot_server", "pxe_server_ip"):
        val = (settings.get(key) or "").strip()
        if val:
            return val
    env = os.environ.get("LEAP_PXE_HTTP_HOST", "").strip()
    if env:
        return env
    try:
        import subprocess

        out = subprocess.check_output(
            ["ip", "-4", "-o", "addr", "show", "scope", "global"],
            text=True,
            stderr=subprocess.DEVNULL,
        )
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 4:
                return parts[3].split("/", 1)[0]
    except Exception:
        pass
    return ""


images = load(STATE_DIR / "images.json", {})
devices = load(STATE_DIR / "devices.json", {})
settings = load(STATE_DIR / "settings.json", {})
default_id = settings.get("default_image_id")
http_host = http_boot_host(settings)
if not http_host:
    print(
        "render-pxe: warning: could not detect HTTP host; set http_boot_server in settings",
        flush=True,
    )


def image_type(meta):
    return meta.get("type", "rtems-multiboot")


def rtems_args(meta):
    return meta.get("kernel_args", DEFAULT_RTEMS_ARGS)


def strip_modloop(base: str) -> str:
    return re.sub(r"\s*modloop=\S+", "", base).strip()


def strip_apkovl(base: str) -> str:
    return re.sub(r"\s*apkovl=\S+", "", base).strip()


def ensure_pxe_modules(base: str) -> str:
    required = ("loop", "squashfs", "r8169")

    def add_mods(match: re.Match[str]) -> str:
        mods = [m for m in match.group(1).split(",") if m]
        for mod in required:
            if mod not in mods:
                mods.append(mod)
        return f"modules={','.join(mods)}"

    if "modules=" in base:
        return re.sub(r"modules=([^\s]+)", add_mods, base, count=1)
    return f"{base} modules=loop,squashfs,sd-mod,ext4,r8169"


def alpine_files(meta, image_id: str | None = None):
    files = {
        "vmlinuz": meta.get("vmlinuz", "vmlinuz-lts"),
        "initramfs": meta.get("initramfs", "initramfs-lts"),
        "modloop": meta.get("modloop", meta.get("filename", "modloop-lts")),
    }
    apkovl = meta.get("apkovl")
    if not apkovl and image_id:
        image_dir = DATA_ROOT / "tftp/images" / image_id
        for candidate in (
            "leap-device.apkovl.tar.gz",
            "leap-device.apkovl.tgz",
        ):
            if (image_dir / candidate).is_file():
                apkovl = candidate
                break
    if apkovl:
        files["apkovl"] = apkovl
    return files


def alpine_append(meta, image_id, http_host: str | None = None):
    files = alpine_files(meta, image_id=image_id)
    repo = meta.get("alpine_repo", DEFAULT_ALPINE_REPO)
    base = strip_apkovl(strip_modloop(meta.get("kernel_args", DEFAULT_ALPINE_APPEND).strip()))
    base = ensure_pxe_modules(base)
    if http_host:
        modloop = f"http://{http_host}/httpboot/images/{image_id}/{files['modloop']}"
    else:
        modloop = f"http://${{next-server}}/httpboot/images/{image_id}/{files['modloop']}"
    parts = [base]
    if "alpine_repo=" not in base:
        parts.insert(0, f"alpine_repo={repo}")
    parts.append(f"modloop={modloop}")
    apkovl_name = files.get("apkovl")
    if apkovl_name:
        if http_host:
            apkovl = f"http://{http_host}/httpboot/images/{image_id}/{apkovl_name}"
        else:
            apkovl = f"http://${{next-server}}/httpboot/images/{image_id}/{apkovl_name}"
        parts.append(f"apkovl={apkovl}")
    elif image_type(meta) == "alpine-linux":
        print(
            f"render-pxe: warning: image {image_id} has no apkovl — "
            "leap-device will not start on PXE boot",
            flush=True,
        )
    return " ".join(parts)


def write_ipxe(path, image_id, label):
    meta = images.get(image_id)
    if not meta:
        return
    itype = image_type(meta)
    if itype == "alpine-linux":
        files = alpine_files(meta, image_id=image_id)
        append = alpine_append(meta, image_id, http_host=http_host or None)
        body = f"""#!ipxe
echo LeapOS NetBoot: {label}
kernel http://${{next-server}}/httpboot/images/{image_id}/{files['vmlinuz']} {append}
initrd http://${{next-server}}/httpboot/images/{image_id}/{files['initramfs']}
boot
"""
    else:
        rel = f"/images/{image_id}/{meta['filename']}"
        args = rtems_args(meta)
        body = f"""#!ipxe
echo LeapOS NetBoot: {label}
kernel http://${{next-server}}{rel} {args}
boot
"""
    path.write_text(body)


# --- GRUB menu ---
grub_lines = [
    "# Generated by render-pxe.sh — do not edit",
    "serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1",
    "terminal_input serial console",
    "terminal_output serial console",
    "set default=0",
    "set timeout=3",
    "set root=(tftp)",
    "",
]
for idx, (iid, meta) in enumerate(sorted(images.items(), key=lambda x: x[1].get("name", ""))):
    title = meta.get("name", iid)
    itype = image_type(meta)
    if itype == "alpine-linux":
        files = alpine_files(meta, iid)
        append = alpine_append(meta, iid, http_host=http_host or None)
        grub_lines.extend([
            f'menuentry "{title}" {{',
            f'    linux /images/{iid}/{files["vmlinuz"]} {append}',
            f'    initrd /images/{iid}/{files["initramfs"]}',
            "    boot",
            "}",
            "",
        ])
    else:
        rel = f"images/{iid}/{meta['filename']}"
        args = rtems_args(meta)
        grub_lines.extend([
            f'menuentry "{title}" {{',
            f'    multiboot /{rel} {args}',
            "    boot",
            "}",
            "",
        ])
if not images:
    grub_lines.extend([
        'menuentry "No images uploaded" {',
        '    echo "Upload images via Web UI at http://<server>/"',
        "}",
        "",
    ])
(GRUB_DIR / "grub.cfg").write_text("\n".join(grub_lines))

# --- iPXE top menu ---
menu_lines = [
    "#!ipxe",
    "echo LeapOS NetBoot iPXE menu",
    "dhcp",
    "chain http://${next-server}/ipxe/by-mac/${mac:hexhyp}.ipxe || chain http://${next-server}/ipxe/default.ipxe",
]
(IPXE_DIR / "menu.ipxe").write_text("\n".join(menu_lines) + "\n")

# --- default.ipxe ---
if default_id and default_id in images:
    write_ipxe(IPXE_DIR / "default.ipxe", default_id, images[default_id].get("name", "default"))
else:
    (IPXE_DIR / "default.ipxe").write_text("#!ipxe\necho No default image configured\nshell\n")

# --- per-MAC scripts ---
for old in BY_MAC_DIR.glob("*.ipxe"):
    old.unlink()
for mac, dev in devices.items():
    iid = dev.get("image_id") or default_id
    if not iid or iid not in images:
        continue
    fname = mac.replace(":", "-") + ".ipxe"
    write_ipxe(BY_MAC_DIR / fname, iid, dev.get("label", mac))

print(f"render-pxe: {len(images)} image(s), {len(devices)} device(s)")
PY
