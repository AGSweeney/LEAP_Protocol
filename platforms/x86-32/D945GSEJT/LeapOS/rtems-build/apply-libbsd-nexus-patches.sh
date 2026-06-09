#!/bin/bash
# LeapOS pc386: fix PCI bushandle on i386 libbsd nexus (re/fxp CSR access).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

find_libbsd_file() {
    find "$RTEMS_ROOT" "$RTEMS_SRC" -path "*/rtems-libbsd-6.2/$1" 2>/dev/null | head -1
}

NEXUS="$(find_libbsd_file rtemsbsd/rtems/rtems-kernel-nexus.c)"

if [ -z "$NEXUS" ]; then
    echo "error: rtems-kernel-nexus.c not found" >&2
    exit 1
fi

python3 - "$NEXUS" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
changed = []

ioport_old = """\tcase SYS_RES_IOPORT:
#ifdef __i386__
\t\trman_set_bustag(res, X86_BUS_SPACE_IO);
#else
\t\trman_set_bushandle(res,
\t\t   rman_get_start(res) + RTEMS_BSP_PCI_IO_REGION_BASE);
#endif
\t\tbreak;"""

ioport_new = """\tcase SYS_RES_IOPORT:
#ifdef __i386__
\t\trman_set_bustag(res, X86_BUS_SPACE_IO);
\t\t/* LeapOS: pc386 needs port/base in bushandle for inb/inl CSR access. */
\t\trman_set_bushandle(res, rman_get_start(res));
#else
\t\trman_set_bushandle(res,
\t\t   rman_get_start(res) + RTEMS_BSP_PCI_IO_REGION_BASE);
#endif
\t\tbreak;"""

mem_old = """\tcase SYS_RES_MEMORY:
#ifdef __i386__
\t\trman_set_bustag(res, X86_BUS_SPACE_MEM);
#else
\t\trman_set_bushandle(res,
\t\t   rman_get_start(res) + RTEMS_BSP_PCI_MEM_REGION_BASE);
#endif
\t\tbreak;"""

mem_new = """\tcase SYS_RES_MEMORY:
#ifdef __i386__
\t\trman_set_bustag(res, X86_BUS_SPACE_MEM);
\t\t/* LeapOS: pc386 needs BAR phys addr in bushandle for MMIO CSR access. */
\t\trman_set_bushandle(res, rman_get_start(res));
#else
\t\trman_set_bushandle(res,
\t\t   rman_get_start(res) + RTEMS_BSP_PCI_MEM_REGION_BASE);
#endif
\t\tbreak;"""

desc_old = 'device_set_desc(dev, "RTEMS Nexus device");'
desc_new = 'device_set_desc(dev, "LeapOS Nexus device");'

if ioport_new not in text:
    if ioport_old not in text:
        raise SystemExit(f"IOPORT activate anchor not found in {path}")
    text = text.replace(ioport_old, ioport_new, 1)
    changed.append("nexus: i386 IOPORT bushandle")

if desc_new not in text:
    if desc_old not in text:
        raise SystemExit(f"nexus device_set_desc anchor not found in {path}")
    text = text.replace(desc_old, desc_new, 1)
    changed.append("nexus: LeapOS device description")

if mem_new not in text:
    if mem_old not in text:
        raise SystemExit(f"MEMORY activate anchor not found in {path}")
    text = text.replace(mem_old, mem_new, 1)
    changed.append("nexus: i386 MEMORY bushandle")

if changed:
    if "LeapOS pc386 nexus patches applied" not in text:
        text = "/* LeapOS pc386 nexus patches applied */\n" + text
    path.write_text(text)
    print("nexus patches updated:", ", ".join(changed))
else:
    print("nexus patches already up to date:", path)
PY

echo "nexus patches done"
