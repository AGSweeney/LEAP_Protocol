#!/usr/bin/env python3
"""LeapOS NetBoot management API (stdlib only)."""

from __future__ import annotations

import cgi
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import uuid
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse

DATA_ROOT = Path(os.environ.get("LEAP_NETBOOT_ROOT", "/var/lib/leap-netboot"))
STATE_DIR = DATA_ROOT / "state"
TFTP_ROOT = DATA_ROOT / "tftp"
IMAGES_DIR = TFTP_ROOT / "images"
INCOMING_DIR = DATA_ROOT / "incoming"
RENDER_SCRIPT = Path("/opt/leap-netboot/scripts/render-pxe.sh")

IMAGES_FILE = STATE_DIR / "images.json"
DEVICES_FILE = STATE_DIR / "devices.json"
SETTINGS_FILE = STATE_DIR / "settings.json"
BOOT_EVENTS_FILE = STATE_DIR / "boot_events.json"

DEFAULT_RTEMS_ARGS = (
    "--video=off --console=/dev/com1,115200 --printk=/dev/com1,115200"
)
DEFAULT_ALPINE_REPO = "http://dl-cdn.alpinelinux.org/alpine/v3.20/main"
DEFAULT_ALPINE_APPEND = (
    "ip=dhcp modules=loop,squashfs,sd-mod,ext4,r8169 earlyprintk=serial,ttyS0,115200 "
    "console=tty1 console=ttyS0,115200n8"
)
ALPINE_VMLINUZ = "vmlinuz-lts"
ALPINE_INITRAMFS = "initramfs-lts"
ALPINE_MODLOOP = "modloop-lts"
ALPINE_APKOVL = "leap-device.apkovl.tar.gz"

MAC_RE = re.compile(r"^([0-9a-f]{2}:){5}[0-9a-f]{2}$")


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def read_json(path: Path, default: Any) -> Any:
    if not path.is_file():
        return default
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, sort_keys=True)
        fh.write("\n")
    tmp.replace(path)


def normalize_mac(value: str) -> str:
    raw = value.strip().lower().replace("-", ":")
    if MAC_RE.match(raw):
        return raw
    hex_only = re.sub(r"[^0-9a-f]", "", raw.lower())
    if len(hex_only) != 12:
        raise ValueError("invalid MAC address")
    return ":".join(hex_only[i : i + 2] for i in range(0, 12, 2))


def mac_filename(mac: str) -> str:
    return mac.replace(":", "-")


def render_pxe() -> None:
    script = RENDER_SCRIPT
    if not script.is_file():
        fallback = Path(__file__).resolve().parent.parent / "scripts" / "render-pxe.sh"
        if fallback.is_file():
            script = fallback
    if script.is_file() and os.access(script, os.X_OK):
        subprocess.run([str(script)], check=False)


def _form_file(form: cgi.FieldStorage, key: str) -> cgi.FieldStorage | None:
    if key not in form:
        return None
    item = form[key]
    if item is None or getattr(item, "file", None) is None:
        return None
    return item


def _form_file_first(form: cgi.FieldStorage, *keys: str) -> cgi.FieldStorage | None:
    for key in keys:
        item = _form_file(form, key)
        if item is not None:
            return item
    return None


def _pick_alpine_name(existing: set[str], default: str, patterns: tuple[str, ...]) -> str:
    for name in existing:
        base = Path(name).name
        for pat in patterns:
            if re.fullmatch(pat, base):
                return base
    return default


def _extract_alpine_archive(archive: Path, dest_dir: Path) -> dict[str, str]:
    dest_dir.mkdir(parents=True, exist_ok=True)
    names: set[str] = set()

    def _add(name: str, src) -> None:
        base = Path(name).name
        if not base or base.startswith(".") or "/" in base or "\\" in base:
            return
        out = dest_dir / base
        with out.open("wb") as fh:
            if hasattr(src, "read"):
                shutil.copyfileobj(src, fh)
            else:
                fh.write(src)
        names.add(base)

    suffixes = "".join(archive.suffixes).lower()
    if suffixes.endswith(".tar.gz") or archive.name.lower().endswith(".tgz"):
        with tarfile.open(archive, "r:gz") as tar:
            for member in tar.getmembers():
                if not member.isfile():
                    continue
                _add(member.name, tar.extractfile(member))
    elif suffixes.endswith(".tar") or archive.name.lower().endswith(".tar"):
        with tarfile.open(archive, "r:") as tar:
            for member in tar.getmembers():
                if not member.isfile():
                    continue
                _add(member.name, tar.extractfile(member))
    else:
        raise ValueError("Alpine bundle must be .tar.gz, .tgz, or .tar")

    required = {
        "vmlinuz": _pick_alpine_name(names, ALPINE_VMLINUZ, (r"vmlinuz.*",)),
        "initramfs": _pick_alpine_name(names, ALPINE_INITRAMFS, (r"initramfs.*",)),
        "modloop": _pick_alpine_name(names, ALPINE_MODLOOP, (r"modloop.*",)),
    }
    apkovl = _pick_alpine_name(names, ALPINE_APKOVL, (r".*apkovl.*\.tar\.gz$", r".*apkovl.*"))
    if apkovl and (dest_dir / apkovl).is_file():
        required["apkovl"] = apkovl
    for key, fname in required.items():
        if not (dest_dir / fname).is_file():
            raise ValueError(f"Alpine upload missing {key} (expected {fname})")
    return required


def _save_uploaded_file(item: cgi.FieldStorage, dest: Path) -> None:
    with dest.open("wb") as out:
        shutil.copyfileobj(item.file, out)


def register_alpine_image(
    *,
    name: str,
    dest_dir: Path,
    vmlinuz: str,
    initramfs: str,
    modloop: str,
    kernel_args: str,
    set_default: bool,
    apkovl: str | None = None,
) -> dict[str, Any]:
    payload = (vmlinuz, initramfs, modloop)
    if apkovl:
        payload = (*payload, apkovl)
    size = sum((dest_dir / fname).stat().st_size for fname in payload)
    image_id = uuid.uuid4().hex[:12]
    final_dir = IMAGES_DIR / image_id
    if final_dir.exists():
        shutil.rmtree(final_dir)
    shutil.move(str(dest_dir), str(final_dir))

    meta = {
        "id": image_id,
        "name": name,
        "type": "alpine-linux",
        "filename": modloop,
        "vmlinuz": vmlinuz,
        "initramfs": initramfs,
        "modloop": modloop,
        "alpine_repo": DEFAULT_ALPINE_REPO,
        "kernel_args": kernel_args.strip() or DEFAULT_ALPINE_APPEND,
        "size_bytes": size,
        "created": utc_now(),
    }
    if apkovl:
        meta["apkovl"] = apkovl
    images = read_json(IMAGES_FILE, {})
    images[image_id] = meta
    write_json(IMAGES_FILE, images)
    if set_default:
        settings = read_json(SETTINGS_FILE, {})
        settings["default_image_id"] = image_id
        write_json(SETTINGS_FILE, settings)
    render_pxe()
    return meta


def disk_free_mb() -> int:
    usage = shutil.disk_usage(DATA_ROOT)
    return usage.free // (1024 * 1024)


def record_boot_event(query: dict[str, list[str]], remote_ip: str) -> dict[str, Any]:
    mac = normalize_mac((query.get("mac") or [""])[0])
    now = utc_now()
    ip = (query.get("ip") or [remote_ip])[0] or remote_ip
    hostname = (query.get("hostname") or [""])[0].strip()
    image_id = (query.get("image") or query.get("image_id") or [""])[0].strip()
    kernel = (query.get("kernel") or [""])[0].strip()

    events = read_json(BOOT_EVENTS_FILE, {})
    prev = events.get(mac, {})
    entry = {
        "mac": mac,
        "ip": ip,
        "hostname": hostname,
        "image_id": image_id,
        "kernel": kernel,
        "first_seen": prev.get("first_seen") or now,
        "last_seen": now,
        "last_remote_ip": remote_ip,
        "boot_count": int(prev.get("boot_count") or 0) + 1,
    }
    events[mac] = entry
    write_json(BOOT_EVENTS_FILE, events)

    devices = read_json(DEVICES_FILE, {})
    device = devices.get(mac, {"mac": mac})
    device.setdefault("label", hostname or mac)
    device.setdefault("notes", "")
    if image_id:
        device.setdefault("image_id", image_id)
    device["last_boot"] = now
    device["last_ip"] = ip
    device["last_hostname"] = hostname
    device["last_image_id"] = image_id
    device["boot_count"] = entry["boot_count"]
    device["updated"] = now
    devices[mac] = device
    write_json(DEVICES_FILE, devices)
    return entry


def booted_devices_report() -> list[dict[str, Any]]:
    images = read_json(IMAGES_FILE, {})
    devices = read_json(DEVICES_FILE, {})
    events = read_json(BOOT_EVENTS_FILE, {})
    rows: list[dict[str, Any]] = []

    for mac in sorted(set(devices) | set(events)):
        dev = devices.get(mac, {"mac": mac})
        event = events.get(mac, {})
        image_id = event.get("image_id") or dev.get("last_image_id") or dev.get("image_id")
        image = images.get(image_id) if image_id else None
        rows.append(
            {
                "mac": mac,
                "label": dev.get("label") or event.get("hostname") or mac,
                "notes": dev.get("notes") or "",
                "ip": event.get("ip") or dev.get("last_ip") or "",
                "hostname": event.get("hostname") or dev.get("last_hostname") or "",
                "image_id": image_id or "",
                "image_name": image.get("name") if image else (image_id or ""),
                "first_seen": event.get("first_seen") or "",
                "last_seen": event.get("last_seen") or dev.get("last_boot") or "",
                "boot_count": event.get("boot_count") or dev.get("boot_count") or 0,
            }
        )

    rows.sort(key=lambda row: row.get("last_seen") or "", reverse=True)
    return rows


class NetbootHandler(BaseHTTPRequestHandler):
    server_version = "LeapNetboot/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send_json(self, status: int, payload: Any) -> None:
        body = json.dumps(payload, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_error_json(self, status: int, message: str) -> None:
        self._send_json(status, {"error": message})

    def _cgi_environ(self) -> dict[str, str]:
        return {
            "REQUEST_METHOD": "POST",
            "CONTENT_TYPE": self.headers.get("Content-Type", ""),
            "CONTENT_LENGTH": self.headers.get("Content-Length", "0"),
        }

    def _read_body(self) -> bytes:
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return b""
        return self.rfile.read(length)

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"

        if path == "/api/v1/status":
            self._send_json(
                HTTPStatus.OK,
                {
                    "server_label": read_json(SETTINGS_FILE, {}).get(
                        "server_label", "LeapOS NetBoot"
                    ),
                    "data_root": str(DATA_ROOT),
                    "tftp_root": str(TFTP_ROOT),
                    "disk_free_mb": disk_free_mb(),
                    "image_count": len(read_json(IMAGES_FILE, {})),
                    "device_count": len(read_json(DEVICES_FILE, {})),
                    "booted_device_count": len(read_json(BOOT_EVENTS_FILE, {})),
                    "dhcp_note": "DHCP is external — configure router options 66/67",
                },
            )
            return

        if path == "/api/v1/settings":
            self._send_json(HTTPStatus.OK, read_json(SETTINGS_FILE, {}))
            return

        if path == "/api/v1/images":
            self._send_json(HTTPStatus.OK, read_json(IMAGES_FILE, {}))
            return

        if path == "/api/v1/devices":
            self._send_json(HTTPStatus.OK, read_json(DEVICES_FILE, {}))
            return

        if path == "/api/v1/booted-devices":
            self._send_json(HTTPStatus.OK, {"devices": booted_devices_report()})
            return

        if path == "/api/v1/boot-event":
            try:
                entry = record_boot_event(parse_qs(parsed.query), self.client_address[0])
            except ValueError as exc:
                self._send_error_json(HTTPStatus.BAD_REQUEST, str(exc))
                return
            self._send_json(HTTPStatus.OK, {"ok": True, "event": entry})
            return

        if path.startswith("/api/v1/images/"):
            image_id = unquote(path.split("/")[-1])
            images = read_json(IMAGES_FILE, {})
            if image_id not in images:
                self._send_error_json(HTTPStatus.NOT_FOUND, "image not found")
                return
            self._send_json(HTTPStatus.OK, images[image_id])
            return

        self._send_error_json(HTTPStatus.NOT_FOUND, "not found")

    def do_PUT(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")

        if path == "/api/v1/settings":
            try:
                body = json.loads(self._read_body().decode("utf-8"))
            except json.JSONDecodeError:
                self._send_error_json(HTTPStatus.BAD_REQUEST, "invalid JSON")
                return
            settings = read_json(SETTINGS_FILE, {})
            for key in ("default_image_id", "server_label", "http_boot_base", "http_boot_server"):
                if key in body:
                    settings[key] = body[key]
            write_json(SETTINGS_FILE, settings)
            render_pxe()
            self._send_json(HTTPStatus.OK, settings)
            return

        if path.startswith("/api/v1/devices/"):
            mac_raw = unquote(path.split("/")[-1])
            try:
                mac = normalize_mac(mac_raw)
            except ValueError as exc:
                self._send_error_json(HTTPStatus.BAD_REQUEST, str(exc))
                return
            try:
                body = json.loads(self._read_body().decode("utf-8"))
            except json.JSONDecodeError:
                self._send_error_json(HTTPStatus.BAD_REQUEST, "invalid JSON")
                return

            devices = read_json(DEVICES_FILE, {})
            entry = devices.get(mac, {"mac": mac})
            for key in ("label", "notes", "image_id"):
                if key in body:
                    entry[key] = body[key]
            entry["updated"] = utc_now()
            devices[mac] = entry
            write_json(DEVICES_FILE, devices)
            render_pxe()
            self._send_json(HTTPStatus.OK, entry)
            return

        self._send_error_json(HTTPStatus.NOT_FOUND, "not found")

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")

        if path == "/api/v1/render":
            render_pxe()
            self._send_json(HTTPStatus.OK, {"ok": True})
            return

        if path == "/api/v1/devices":
            try:
                body = json.loads(self._read_body().decode("utf-8"))
            except json.JSONDecodeError:
                self._send_error_json(HTTPStatus.BAD_REQUEST, "invalid JSON")
                return
            try:
                mac = normalize_mac(body.get("mac", ""))
            except ValueError as exc:
                self._send_error_json(HTTPStatus.BAD_REQUEST, str(exc))
                return
            devices = read_json(DEVICES_FILE, {})
            entry = {
                "mac": mac,
                "label": body.get("label", mac),
                "notes": body.get("notes", ""),
                "image_id": body.get("image_id"),
                "updated": utc_now(),
            }
            devices[mac] = entry
            write_json(DEVICES_FILE, devices)
            render_pxe()
            self._send_json(HTTPStatus.CREATED, entry)
            return

        if path == "/api/v1/images":
            self._handle_image_upload()
            return

        self._send_error_json(HTTPStatus.NOT_FOUND, "not found")

    def do_DELETE(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")

        if path.startswith("/api/v1/devices/"):
            mac_raw = unquote(path.split("/")[-1])
            try:
                mac = normalize_mac(mac_raw)
            except ValueError as exc:
                self._send_error_json(HTTPStatus.BAD_REQUEST, str(exc))
                return
            devices = read_json(DEVICES_FILE, {})
            if mac not in devices:
                self._send_error_json(HTTPStatus.NOT_FOUND, "device not found")
                return
            del devices[mac]
            write_json(DEVICES_FILE, devices)
            render_pxe()
            self._send_json(HTTPStatus.OK, {"deleted": mac})
            return

        if path.startswith("/api/v1/images/"):
            image_id = unquote(path.split("/")[-1])
            images = read_json(IMAGES_FILE, {})
            if image_id not in images:
                self._send_error_json(HTTPStatus.NOT_FOUND, "image not found")
                return
            meta = images.pop(image_id)
            write_json(IMAGES_FILE, images)
            settings = read_json(SETTINGS_FILE, {})
            if settings.get("default_image_id") == image_id:
                settings["default_image_id"] = None
                write_json(SETTINGS_FILE, settings)
            image_dir = IMAGES_DIR / image_id
            if image_dir.is_dir():
                shutil.rmtree(image_dir)
            render_pxe()
            self._send_json(HTTPStatus.OK, {"deleted": image_id, "meta": meta})
            return

        self._send_error_json(HTTPStatus.NOT_FOUND, "not found")

    def _handle_image_upload(self) -> None:
        try:
            self._handle_image_upload_inner()
        except Exception as exc:
            sys.stderr.write(f"image upload failed: {exc}\n")
            msg = str(exc)
            if "No space left on device" in msg or getattr(exc, "errno", None) == 28:
                msg = (
                    f"{msg} — uploads must use /var/lib/leap-netboot/incoming "
                    f"(not /tmp). Restart leap-netboot after updating the image."
                )
            self._send_error_json(
                HTTPStatus.INTERNAL_SERVER_ERROR, f"upload failed: {msg}"
            )

    def _handle_image_upload_inner(self) -> None:
        content_type = self.headers.get("Content-Type", "")
        if not content_type.startswith("multipart/form-data"):
            self._send_error_json(
                HTTPStatus.BAD_REQUEST, "expected multipart/form-data"
            )
            return

        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            self._send_error_json(HTTPStatus.BAD_REQUEST, "missing Content-Length")
            return

        free = shutil.disk_usage(DATA_ROOT).free
        if length + (200 * 1024 * 1024) > free:
            self._send_error_json(
                HTTPStatus.INSUFFICIENT_STORAGE,
                f"not enough disk space for upload ({length // (1024 * 1024)} MiB)",
            )
            return

        form = cgi.FieldStorage(
            fp=self.rfile,
            headers=self.headers,
            environ=self._cgi_environ(),
        )

        name = form.getfirst("name") or "image"
        image_type = form.getfirst("type") or "rtems-multiboot"
        kernel_args = form.getfirst("kernel_args") or DEFAULT_RTEMS_ARGS
        set_default = form.getfirst("set_default", "0") in ("1", "true", "yes")

        if image_type == "alpine-linux":
            try:
                meta = self._handle_alpine_upload(form, name, kernel_args, set_default)
            except ValueError as exc:
                self._send_error_json(HTTPStatus.BAD_REQUEST, str(exc))
                return
            except OSError as exc:
                self._send_error_json(
                    HTTPStatus.INSUFFICIENT_STORAGE, f"storage error: {exc}"
                )
                return
            self._send_json(HTTPStatus.CREATED, meta)
            return

        file_item = _form_file(form, "file")
        if file_item is None:
            self._send_error_json(HTTPStatus.BAD_REQUEST, "missing file field")
            return

        if not name:
            name = getattr(file_item, "filename", "") or "image"
        original = Path(getattr(file_item, "filename", "") or "payload.bin").name
        image_id = uuid.uuid4().hex[:12]
        dest_dir = IMAGES_DIR / image_id
        dest_dir.mkdir(parents=True, exist_ok=True)
        dest_file = dest_dir / original

        _save_uploaded_file(file_item, dest_file)

        size = dest_file.stat().st_size
        images = read_json(IMAGES_FILE, {})
        images[image_id] = {
            "id": image_id,
            "name": name,
            "type": image_type,
            "filename": original,
            "kernel_args": kernel_args.strip(),
            "size_bytes": size,
            "created": utc_now(),
        }
        write_json(IMAGES_FILE, images)

        if set_default:
            settings = read_json(SETTINGS_FILE, {})
            settings["default_image_id"] = image_id
            write_json(SETTINGS_FILE, settings)

        render_pxe()
        self._send_json(HTTPStatus.CREATED, images[image_id])

    def _handle_alpine_upload(
        self,
        form: cgi.FieldStorage,
        name: str,
        kernel_args: str,
        set_default: bool,
    ) -> dict[str, Any]:
        staging = INCOMING_DIR / f"alpine-{uuid.uuid4().hex[:12]}"
        staging.mkdir(parents=True, exist_ok=True)
        apkovl: str | None = None
        try:
            vmlinuz_item = _form_file(form, "vmlinuz")
            initramfs_item = _form_file(form, "initramfs")
            modloop_item = _form_file(form, "modloop")
            bundle_item = _form_file_first(form, "file", "bundle")

            if (
                vmlinuz_item is not None
                and initramfs_item is not None
                and modloop_item is not None
            ):
                vmlinuz = Path(getattr(vmlinuz_item, "filename", "") or ALPINE_VMLINUZ).name
                initramfs = Path(
                    getattr(initramfs_item, "filename", "") or ALPINE_INITRAMFS
                ).name
                modloop = Path(getattr(modloop_item, "filename", "") or ALPINE_MODLOOP).name
                _save_uploaded_file(vmlinuz_item, staging / vmlinuz)
                _save_uploaded_file(initramfs_item, staging / initramfs)
                _save_uploaded_file(modloop_item, staging / modloop)
            elif bundle_item is not None:
                if not name:
                    name = getattr(bundle_item, "filename", "") or "LeapOS device Alpine"
                archive = staging / Path(getattr(bundle_item, "filename", "") or "bundle.tar.gz").name
                _save_uploaded_file(bundle_item, archive)
                picked = _extract_alpine_archive(archive, staging)
                archive.unlink(missing_ok=True)
                vmlinuz = picked["vmlinuz"]
                initramfs = picked["initramfs"]
                modloop = picked["modloop"]
                apkovl = picked.get("apkovl")
            else:
                raise ValueError(
                    "Alpine upload needs leap-device-alpine-pxe.tar.gz or "
                    "vmlinuz + initramfs + modloop files"
                )

            if not kernel_args.strip():
                kernel_args = DEFAULT_ALPINE_APPEND

            return register_alpine_image(
                name=name,
                dest_dir=staging,
                vmlinuz=vmlinuz,
                initramfs=initramfs,
                modloop=modloop,
                kernel_args=kernel_args,
                set_default=set_default,
                apkovl=apkovl,
            )
        except Exception:
            if staging.is_dir():
                shutil.rmtree(staging, ignore_errors=True)
            raise


def main() -> None:
    for path in (STATE_DIR, TFTP_ROOT, IMAGES_DIR, INCOMING_DIR):
        path.mkdir(parents=True, exist_ok=True)
    upload_tmp = INCOMING_DIR / "tmp"
    upload_tmp.mkdir(parents=True, exist_ok=True)
    os.environ["TMPDIR"] = str(upload_tmp)

    host = os.environ.get("LEAP_NETBOOT_BIND", "127.0.0.1")
    port = int(os.environ.get("LEAP_NETBOOT_PORT", "8081"))
    server = ThreadingHTTPServer((host, port), NetbootHandler)
    print(f"leap_netbootd listening on {host}:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
