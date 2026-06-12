#!/bin/bash
# Embed LeapGateway/web/index.html (+ style.css) as a C translation unit.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_HTML="$GATEWAY_DIR/web/index.html"
SRC_CSS="$GATEWAY_DIR/web/style.css"
SRC_LOGO="$GATEWAY_DIR/web/leapos-logo.png"
OUT_C="$GATEWAY_DIR/src/gateway_web_index.c"
OUT_H="$GATEWAY_DIR/src/gateway_web_index.h"

if [ ! -f "$SRC_HTML" ]; then
	echo "missing $SRC_HTML" >&2
	exit 1
fi

python3 - "$SRC_HTML" "$SRC_CSS" "$SRC_LOGO" "$OUT_C" "$OUT_H" <<'PY'
import base64
import sys

src_path, css_path, logo_path, out_c, out_h = sys.argv[1:6]
html = open(src_path, "r", encoding="utf-8").read()

link_tag = '<link rel="stylesheet" href="style.css">'
if link_tag in html:
    try:
        css = open(css_path, "r", encoding="utf-8").read()
    except OSError:
        css = ""
    if css:
        html = html.replace(link_tag, f"<style>\n{css}\n</style>")

logo_token = "__LEAPOS_LOGO_DATA_URI__"
if logo_token in html:
    try:
        with open(logo_path, "rb") as f:
            logo_b64 = base64.b64encode(f.read()).decode("ascii")
    except OSError as exc:
        raise SystemExit(f"missing LeapOS logo: {logo_path}: {exc}")
    html = html.replace(logo_token, f"data:image/png;base64,{logo_b64}")

non_ascii = sorted({ch for ch in html if ord(ch) > 127})
if non_ascii:
    sample = ", ".join(f"U+{ord(ch):04X}" for ch in non_ascii[:8])
    raise SystemExit(
        f"web UI must be ASCII-only for embedded gateway (found: {sample})"
    )

lines = []
for ch in html:
    if ch == "\\":
        lines.append("\\\\")
    elif ch == "\"":
        lines.append("\\\"")
    elif ch == "\n":
        lines.append("\\n")
    elif ch == "\r":
        lines.append("\\r")
    elif ch == "\t":
        lines.append("\\t")
    elif ord(ch) < 32 or ord(ch) > 126:
        lines.append("\\x%02x" % ord(ch))
    else:
        lines.append(ch)

body = "".join(lines)

open(out_h, "w", encoding="utf-8").write(
    "/* Auto-generated from web/index.html — do not edit. */\n"
    "#ifndef LEAP_GATEWAY_WEB_INDEX_H\n"
    "#define LEAP_GATEWAY_WEB_INDEX_H\n\n"
    "#include <stddef.h>\n\n"
    "extern const char leap_gateway_web_index_html[];\n"
    "extern const size_t leap_gateway_web_index_html_len;\n\n"
    "#endif\n"
)

open(out_c, "w", encoding="utf-8").write(
    "/* Auto-generated from web/index.html — do not edit. */\n"
    "#include \"gateway_web_index.h\"\n\n"
    "const char leap_gateway_web_index_html[] =\n"
    f"    \"{body}\";\n\n"
    f"const size_t leap_gateway_web_index_html_len = "
    f"sizeof(leap_gateway_web_index_html) - 1u;\n"
)
PY

echo "Generated $OUT_C"
