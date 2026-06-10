#!/bin/bash
# Embed LeapGateway/web/index.html as a C translation unit.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_HTML="$GATEWAY_DIR/web/index.html"
OUT_C="$GATEWAY_DIR/src/gateway_web_index.c"
OUT_H="$GATEWAY_DIR/src/gateway_web_index.h"

if [ ! -f "$SRC_HTML" ]; then
	echo "missing $SRC_HTML" >&2
	exit 1
fi

python3 - "$SRC_HTML" "$OUT_C" "$OUT_H" <<'PY'
import sys

src_path, out_c, out_h = sys.argv[1:4]
data = open(src_path, "r", encoding="utf-8").read()

lines = []
for ch in data:
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
