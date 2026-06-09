#!/bin/bash
# LeapOS pc386 tweaks for Intel PRO/100 (fxp) on RTEMS libbsd.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

find_libbsd_src() {
    find "$RTEMS_ROOT" "$RTEMS_SRC" -path '*/rtems-libbsd-6.2/freebsd/sys/dev/fxp/if_fxp.c' 2>/dev/null | head -1
}

FXP="$(find_libbsd_src)"
if [ -z "$FXP" ]; then
    echo "error: rtems-libbsd if_fxp.c not found" >&2
    exit 1
fi

if grep -q "LeapOS pc386 fxp patches applied" "$FXP"; then
    echo "fxp patches already applied: $FXP"
    exit 0
fi

cp "$FXP" "${FXP}.leapos-bak"

python3 - "$FXP" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
marker = "/* LeapOS pc386 fxp patches applied */"

old_prefer = """\tprefer_iomap = 0;
\tresource_int_value(device_get_name(dev), device_get_unit(dev),
\t    \"prefer_iomap\", &prefer_iomap);
\tif (prefer_iomap)"""

new_prefer = """\tprefer_iomap = 0;
\tresource_int_value(device_get_name(dev), device_get_unit(dev),
\t    \"prefer_iomap\", &prefer_iomap);
#ifdef __rtems__
\t/* pc386 PRO/100: always use I/O CSRs; hints alone are unreliable. */
\tprefer_iomap = 1;
#endif
\tif (prefer_iomap)"""

old_map_print = """\tif (bootverbose) {
\t\tdevice_printf(dev, \"using %s space register mapping\\n\",
\t\t   sc->fxp_spec == fxp_res_spec_mem ? \"memory\" : \"I/O\");
\t}"""

new_map_print = """\tdevice_printf(dev, \"CSR mapping: %s\\n\",
\t    sc->fxp_spec == fxp_res_spec_mem ? \"memory\" : \"I/O\");"""

old_delay = """\tCSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
\tDELAY(10);
\t/* Full reset and disable interrupts. */
\tCSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SOFTWARE_RESET);
\tDELAY(10);"""

new_delay = """\tCSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
\tDELAY(10000);
\t/* Full reset and disable interrupts. */
\tCSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SOFTWARE_RESET);
\tDELAY(10000);"""

old_stb = """\tif ((sc->ident->ich >= 2 && sc->ident->ich <= 3) ||
\t    (sc->ident->ich == 0 && sc->revision >= FXP_REV_82559_A0)) {
\t\tdata = sc->eeprom[FXP_EEPROM_MAP_ID];
\t\tif (data & 0x02) {\t\t\t/* STB enable */"""

new_stb = """#ifndef __rtems__
\tif ((sc->ident->ich >= 2 && sc->ident->ich <= 3) ||
\t    (sc->ident->ich == 0 && sc->revision >= FXP_REV_82559_A0)) {
\t\tdata = sc->eeprom[FXP_EEPROM_MAP_ID];
\t\tif (data & 0x02) {\t\t\t/* STB enable */"""

old_stb_end = """\t\t\tsc->flags |= FXP_FLAG_CU_RESUME_BUG;
\t\t}
\t}

\t/*
\t * If we are not a 82557 chip, we can enable extended features.
\t */"""

new_stb_end = """\t\t\tsc->flags |= FXP_FLAG_CU_RESUME_BUG;
\t\t}
\t}
#endif /* !__rtems__ */

\t/*
\t * If we are not a 82557 chip, we can enable extended features.
\t */"""

old_mac = """\teaddr[4] = sc->eeprom[FXP_EEPROM_MAP_IA2] & 0xff;
\teaddr[5] = sc->eeprom[FXP_EEPROM_MAP_IA2] >> 8;
\tif (bootverbose) {"""

new_mac = """\teaddr[4] = sc->eeprom[FXP_EEPROM_MAP_IA2] & 0xff;
\teaddr[5] = sc->eeprom[FXP_EEPROM_MAP_IA2] >> 8;
\tif ((eaddr[0] & eaddr[1] & eaddr[2] & eaddr[3] & eaddr[4] & eaddr[5]) == 0xff) {
\t\tdevice_printf(dev,
\t    \"invalid MAC ff:ff:ff:ff:ff:ff — EEPROM/CSR read failed\\n\");
\t\terror = ENXIO;
\t\tgoto fail;
\t}
\tif (bootverbose) {"""

replacements = [
    (old_prefer, new_prefer),
    (old_map_print, new_map_print),
    (old_delay, new_delay),
    (old_stb, new_stb),
    (old_stb_end, new_stb_end),
    (old_mac, new_mac),
]

for old, new in replacements:
    if old not in text:
        raise SystemExit(f"patch anchor not found in {path}")
    text = text.replace(old, new, 1)

text = marker + "\n" + text
path.write_text(text)
print(f"patched {path}")
PY

echo "fxp patches applied"
