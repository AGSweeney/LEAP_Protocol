#!/bin/bash
# LeapOS pc386 tweaks for onboard RTL8111D/DL (re driver) on RTEMS libbsd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

find_libbsd_file() {
    find "$RTEMS_ROOT" "$RTEMS_SRC" -path "*/rtems-libbsd-6.2/$1" 2>/dev/null | head -1
}

RE="$(find_libbsd_file freebsd/sys/dev/re/if_re.c)"
RLREG="$(find_libbsd_file freebsd/sys/dev/rl/if_rlreg.h)"

if [ -z "$RE" ] || [ -z "$RLREG" ]; then
    echo "error: rtems-libbsd re driver sources not found" >&2
    exit 1
fi

python3 - "$RE" "$RLREG" <<'PY'
import sys
from pathlib import Path

re_path = Path(sys.argv[1])
rlreg_path = Path(sys.argv[2])
changed = []

rlreg = rlreg_path.read_text()
if "RL_HWREV_8168D_D945GSEJT" not in rlreg:
    old = "#define\tRL_HWREV_8168D\t\t0x28000000\n"
    new = old + (
        "/* D945GSEJT RTL8111D/DL observed CSR hwrev values */\n"
        "#define\tRL_HWREV_8168D_D945GSEJT\t0x40000000\n"
        "#define\tRL_HWREV_8168D_D945GSEJT_IO\t0x5c400000\n"
        "#define\tRL_HWREV_8168D_D945GSEJT_IO2\t0x78400000\n"
    )
    if old not in rlreg:
        raise SystemExit(f"8168D anchor not found in {rlreg_path}")
    rlreg = rlreg.replace(old, new, 1)
    changed.append("rlreg: add D945GSEJT rev IDs")

for define, val in (
    ("RL_HWREV_8168D_D945GSEJT_IO", "0x5c400000"),
    ("RL_HWREV_8168D_D945GSEJT_IO2", "0x78400000"),
):
    if define not in rlreg:
        old = "#define\tRL_HWREV_8168D_D945GSEJT\t0x40000000\n"
        extra = (
            f"#define\tRL_HWREV_8168D_D945GSEJT_IO\t0x5c400000\n"
            f"#define\tRL_HWREV_8168D_D945GSEJT_IO2\t0x78400000\n"
        )
        if "RL_HWREV_8168D_D945GSEJT_IO2" not in rlreg and old in rlreg:
            if "RL_HWREV_8168D_D945GSEJT_IO" in rlreg:
                rlreg = rlreg.replace(
                    "#define\tRL_HWREV_8168D_D945GSEJT_IO\t0x5c400000\n",
                    "#define\tRL_HWREV_8168D_D945GSEJT_IO\t0x5c400000\n"
                    "#define\tRL_HWREV_8168D_D945GSEJT_IO2\t0x78400000\n",
                    1,
                )
            else:
                rlreg = rlreg.replace(old, old + extra, 1)
            changed.append(f"rlreg: add {define}")

if "#ifdef __rtems__\n#define\tRL_TIMEOUT" not in rlreg and "#define\tRL_TIMEOUT\t\t1000" in rlreg:
    rlreg = rlreg.replace(
        "#define\tRL_TIMEOUT\t\t1000",
        "#ifdef __rtems__\n#define\tRL_TIMEOUT\t\t10000\n#else\n#define\tRL_TIMEOUT\t\t1000\n#endif",
        1,
    )
    changed.append("rlreg: longer RL_TIMEOUT on RTEMS")

if changed:
    if "LeapOS pc386 re patches applied" not in rlreg:
        rlreg = "/* LeapOS pc386 re patches applied */\n" + rlreg
    rlreg_path.write_text(rlreg)

text = re_path.read_text()

if "8168D/8111DL (D945GSEJT)" not in text:
    old_table = '\t{ RL_HWREV_8168D, RL_8169, "8168D/8111D", RL_JUMBO_MTU_9K },\n'
    new_table = old_table + (
        '\t{ RL_HWREV_8168D_D945GSEJT, RL_8169, "8168D/8111DL (D945GSEJT)", '
        'RL_JUMBO_MTU_9K },\n'
    )
    if old_table not in text:
        raise SystemExit(f"re_hwrevs anchor not found in {re_path}")
    text = text.replace(old_table, new_table, 1)
    changed.append("re: table entry D945GSEJT")

for rev, label in (
    ("RL_HWREV_8168D_D945GSEJT_IO", "8168D/8111DL IO (D945GSEJT)"),
    ("RL_HWREV_8168D_D945GSEJT_IO2", "8168D/8111DL IO2 (D945GSEJT)"),
):
    needle = f'\t{{ {rev}, RL_8169, "{label}", RL_JUMBO_MTU_9K }},\n'
    if label.split()[0] == "8168D/8111DL" and needle not in text:
        if rev == "RL_HWREV_8168D_D945GSEJT_IO":
            anchor = (
                '\t{ RL_HWREV_8168D_D945GSEJT, RL_8169, '
                '"8168D/8111DL (D945GSEJT)", RL_JUMBO_MTU_9K },\n'
            )
        else:
            anchor = (
                '\t{ RL_HWREV_8168D_D945GSEJT_IO, RL_8169, '
                '"8168D/8111DL IO (D945GSEJT)", RL_JUMBO_MTU_9K },\n'
            )
        if anchor in text:
            text = text.replace(anchor, anchor + needle, 1)
            changed.append(f"re: table entry {rev}")

# Remove forced I/O mapping — MMIO BAR2 with fixed nexus bushandle is correct.
attach_iomap = """#ifdef __rtems__
\t/* pc386 D945GSEJT: use I/O BAR for RTL8168 CSR access. */
\tif (devid == RT_DEVICEID_8168)
\t\tprefer_iomap = 1;
#endif
"""
if attach_iomap in text:
    text = text.replace(attach_iomap, "", 1)
    changed.append("re: remove forced prefer_iomap in attach")

legacy_probe_iomap = """#ifdef __rtems__
\t/* pc386 D945GSEJT: MMIO reads bogus hwrev; I/O BAR works. */
\tif (devid == RT_DEVICEID_8168)
\t\tprefer_iomap = 1;
#endif
"""
if legacy_probe_iomap in text:
    text = text.replace(legacy_probe_iomap, "", 1)
    changed.append("re: remove legacy probe prefer_iomap")

bushandle_anchor = "\tsc->rl_bhandle = rman_get_bushandle(sc->rl_res);\n"
rtems_map_dbg = """\tsc->rl_bhandle = rman_get_bushandle(sc->rl_res);
#ifdef __rtems__
\tdevice_printf(dev, "CSR map: %s rid %d start %jx handle %jx\\n",
\t    sc->rl_res_type == SYS_RES_IOPORT ? "I/O" : "MEM",
\t    sc->rl_res_id, (uintmax_t)rman_get_start(sc->rl_res),
\t    (uintmax_t)sc->rl_bhandle);
\tDELAY(10000);
#endif
"""
old_dbg = """\tsc->rl_bhandle = rman_get_bushandle(sc->rl_res);
#ifdef __rtems__
\tdevice_printf(dev, "CSR map: %s res %d handle %jx\\n",
\t    sc->rl_res_type == SYS_RES_IOPORT ? "I/O" : "MEM",
\t    sc->rl_res_id, (uintmax_t)sc->rl_bhandle);
\tDELAY(10000);
#endif
"""
if old_dbg in text:
    text = text.replace(old_dbg, rtems_map_dbg, 1)
    changed.append("re: improve CSR map debug")
elif "CSR map:" not in text and bushandle_anchor in text:
    text = text.replace(bushandle_anchor, rtems_map_dbg, 1)
    changed.append("re: attach CSR map debug + settle delay")

if "case RL_HWREV_8168D_D945GSEJT:" not in text:
    old_switch = "\tcase RL_HWREV_8168D:\n"
    new_switch = (
        "\tcase RL_HWREV_8168D_D945GSEJT:\n"
        "\tcase RL_HWREV_8168D_D945GSEJT_IO:\n"
        "\tcase RL_HWREV_8168D_D945GSEJT_IO2:\n"
        "\t\t/* FALLTHROUGH */\n"
        "\tcase RL_HWREV_8168D:\n"
    )
    if old_switch not in text:
        raise SystemExit(f"8168D switch anchor not found in {re_path}")
    text = text.replace(old_switch, new_switch, 1)
    changed.append("re: switch cases D945GSEJT")
elif "case RL_HWREV_8168D_D945GSEJT_IO2:" not in text:
    old_switch = "\tcase RL_HWREV_8168D_D945GSEJT_IO:\n"
    new_switch = (
        "\tcase RL_HWREV_8168D_D945GSEJT_IO:\n"
        "\tcase RL_HWREV_8168D_D945GSEJT_IO2:\n"
    )
    if old_switch in text:
        text = text.replace(old_switch, new_switch, 1)
        changed.append("re: switch case D945GSEJT_IO2")

unknown_block = """\tif (hw_rev->rl_desc == NULL) {
\t\tdevice_printf(dev, "Unknown H/W revision: 0x%08x\\n", hwrev);
\t\terror = ENXIO;
\t\tgoto fail;
\t}"""
unknown_fallback = """\tif (hw_rev->rl_desc == NULL) {
#ifdef __rtems__
\t\tif (devid == RT_DEVICEID_8168) {
\t\t\tdevice_printf(dev,
\t\t\t    "Unknown H/W revision: 0x%08x — forcing 8168D profile\\n",
\t\t\t    hwrev);
\t\t\thw_rev = re_hwrevs;
\t\t\twhile (hw_rev->rl_desc != NULL) {
\t\t\t\tif (hw_rev->rl_rev == RL_HWREV_8168D) {
\t\t\t\t\tsc->rl_type = hw_rev->rl_type;
\t\t\t\t\tsc->rl_hwrev = hw_rev;
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t\thw_rev++;
\t\t\t}
\t\t}
#endif
\t\tif (hw_rev->rl_desc == NULL) {
\t\t\tdevice_printf(dev, "Unknown H/W revision: 0x%08x\\n", hwrev);
\t\t\terror = ENXIO;
\t\t\tgoto fail;
\t\t}
\t}"""
if "forcing 8168D profile" not in text:
    if unknown_block not in text:
        raise SystemExit(f"unknown hwrev anchor not found in {re_path}")
    text = text.replace(unknown_block, unknown_fallback, 1)
    changed.append("re: RTEMS 8168 unknown hwrev fallback")

if changed:
    if "LeapOS pc386 re patches applied" not in text:
        text = "/* LeapOS pc386 re patches applied */\n" + text
    re_path.write_text(text)
    print("re patches updated:", ", ".join(changed))
else:
    print("re patches already up to date:", re_path)
PY

echo "re patches done"
