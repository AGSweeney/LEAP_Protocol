#!/bin/bash
# CF-via-IDE: LBA force, ghost IDENTIFY reject, ICH7 SATA0 prep before IDE0 probe.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "$SCRIPT_DIR/env.sh"

ATA_UTIL="${RTEMS_SRC}/rtems/bsps/shared/dev/ide/ata_util.c"

if [ ! -f "$ATA_UTIL" ]; then
	ATA_UTIL="$(find "$RTEMS_ROOT" "$RTEMS_SRC" -path '*/bsps/shared/dev/ide/ata_util.c' 2>/dev/null | head -1)"
fi

if [ -z "$ATA_UTIL" ] || [ ! -f "$ATA_UTIL" ]; then
	echo "error: ata_util.c not found under $RTEMS_SRC" >&2
	exit 1
fi

LBA_MARKER="LeapOS: force LBA for CF cards"
GHOST_MARKER="LeapOS: reject ghost IDENTIFY"

python3 - "$ATA_UTIL" "$LBA_MARKER" "$GHOST_MARKER" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
lba_marker = sys.argv[2]
ghost_marker = sys.argv[3]
text = path.read_text()
changed = False

lba_anchor = """  device_entry->lba_avaible =
      (CF_LE_W(sector_buffer[ATA_IDENT_WORD_CAPABILITIES]) >> 9) & 0x1;

  if ((CF_LE_W(sector_buffer[ATA_IDENT_WORD_FIELD_VALIDITY]) &
"""

lba_insert = """  device_entry->lba_avaible =
      (CF_LE_W(sector_buffer[ATA_IDENT_WORD_CAPABILITIES]) >> 9) & 0x1;

  /* """ + lba_marker + """ */
  if (!device_entry->lba_avaible && device_entry->lba_sectors > 0u) {
    device_entry->lba_avaible = 1;
  }

  if ((CF_LE_W(sector_buffer[ATA_IDENT_WORD_FIELD_VALIDITY]) &
"""

if lba_marker not in text:
    if lba_anchor not in text:
        raise SystemExit(f"LBA anchor not found in {path}")
    text = text.replace(lba_anchor, lba_insert, 1)
    changed = True
    print(f"IDE CF LBA patch applied: {path}")

ghost_anchor = """  if ((CF_LE_W(sector_buffer[ATA_IDENT_WORD_FIELD_VALIDITY]) &
       ATA_IDENT_BIT_VALID) == 0) {
    /* no "supported modes" info -> use default */
    device_entry->mode_active = ATA_MODES_PIO3;
  } else {
"""

ghost_insert = """  /* """ + ghost_marker + """ (SATA/AHCI not mapped to legacy IDE) */
  {
    unsigned wi;
    unsigned bad = 0;
    unsigned printable = 0;

    for (wi = 10; wi < 256; wi++) {
      uint16_t w = CF_LE_W(sector_buffer[wi]);

      if (w == 0x7fff || w == 0xff7f || w == 0xffff) {
        bad++;
      }
    }
    for (wi = 27; wi <= 46; wi++) {
      uint16_t w = CF_LE_W(sector_buffer[wi]);
      char c0 = (char)(w >> 8);
      char c1 = (char)(w & 0xff);

      if (c0 >= 0x20 && c0 < 0x7f) {
        printable++;
      }
      if (c1 >= 0x20 && c1 < 0x7f) {
        printable++;
      }
    }
    if (bad > 200 || printable < 4) {
      return RTEMS_IO_ERROR;
    }
  }

  if ((CF_LE_W(sector_buffer[ATA_IDENT_WORD_FIELD_VALIDITY]) &
       ATA_IDENT_BIT_VALID) == 0) {
    /* no "supported modes" info -> use default */
    device_entry->mode_active = ATA_MODES_PIO3;
  } else {
"""

if ghost_marker not in text:
    if ghost_anchor not in text:
        raise SystemExit(f"ghost IDENTIFY anchor not found in {path}")
    text = text.replace(ghost_anchor, ghost_insert, 1)
    changed = True
    print(f"IDE CF ghost IDENTIFY patch applied: {path}")

if not changed:
    print(f"IDE CF patches already applied: {path}")
else:
    path.write_text(text)
PY

OBJIDE="${RTEMS_SRC}/rtems/spec/build/bsps/i386/pc386/objide.yml"
IDE_C="${RTEMS_SRC}/rtems/bsps/i386/pc386/ata/ide.c"
BSP_ATA_DIR="${RTEMS_SRC}/rtems/bsps/i386/pc386/ata"
BSPSTART="${RTEMS_SRC}/rtems/bsps/i386/pc386/start/bspstart.c"
ICH7_MARKER="LeapOS: ICH7 SATA0 prep before IDE0 probe"
SRST_MARKER="LeapOS: skip IDE SRST after SATA0 prep"

python3 - "$SCRIPT_DIR" "$OBJIDE" "$IDE_C" "$BSP_ATA_DIR" "$BSPSTART" "$ICH7_MARKER" "$SRST_MARKER" <<'PY'
import shutil
import sys
from pathlib import Path

script_dir = Path(sys.argv[1])
objide = Path(sys.argv[2])
ide_c = Path(sys.argv[3])
bsp_ata_dir = Path(sys.argv[4])
bspstart = Path(sys.argv[5])
marker = sys.argv[6]
srst_marker = sys.argv[7]

src_c = script_dir / "leapos_ich7_sata.c"
src_h = script_dir / "leapos_ich7_sata.h"
if not src_c.is_file() or not src_h.is_file():
    raise SystemExit(f"missing {src_c} or {src_h}")

bsp_ata_dir.mkdir(parents=True, exist_ok=True)
shutil.copy2(src_c, bsp_ata_dir / "leapos_ich7_sata.c")
shutil.copy2(src_h, bsp_ata_dir / "leapos_ich7_sata.h")
print(f"ICH7 SATA sources installed: {bsp_ata_dir}")

ich7_line = "- bsps/i386/pc386/ata/leapos_ich7_sata.c\n"
obj_text = objide.read_text()
if ich7_line not in obj_text:
    anchor = "- bsps/i386/pc386/ata/ide.c\n"
    if anchor not in obj_text:
        raise SystemExit(f"objide anchor not found in {objide}")
    obj_text = obj_text.replace(anchor, anchor + ich7_line, 1)
    objide.write_text(obj_text)
    print(f"ICH7 SATA source linked in {objide}")
else:
    print(f"ICH7 SATA source already in {objide}")

text = bspstart.read_text()
include_line = '#include "../ata/leapos_ich7_sata.h"\n'
if include_line in text:
    text = text.replace(include_line, "")

ide_insert = f"""#if BSP_ENABLE_IDE
  leapos_ich7_sata0_prep(); /* {marker} */
  bsp_ide_cmdline_init();
#endif"""
ide_anchor = """#if BSP_ENABLE_IDE
  bsp_ide_cmdline_init();
#endif"""
if ide_insert in text:
    text = text.replace(ide_insert, ide_anchor, 1)
    bspstart.write_text(text)
    print(f"Removed legacy ICH7 hook from {bspstart}")

include_anchor = '#include <libchip/ide_ctrl_io.h>\n'
include_insert = include_anchor + '#include "leapos_ich7_sata.h"\n'
text = ide_c.read_text()
if include_insert not in text:
    if include_anchor not in text:
        raise SystemExit(f"ide.c include anchor not found in {ide_c}")
    text = text.replace(include_anchor, include_insert, 1)

prep_anchor = """  uint32_t port = IDE_Controller_Table[minor].port1;
  uint8_t  dev = 0;

  if (pc386_ide_show)
    printk("IDE%d: port base: %04" PRIu32 "\\n", minor, port);
"""
prep_insert = f"""  uint32_t port = IDE_Controller_Table[minor].port1;
  uint8_t  dev = 0;

  if (minor == 0) {{
    (void) leapos_ich7_sata0_prep(); /* {marker} */
    Wait_X_ms(100);
  }}

  if (pc386_ide_show)
    printk("IDE%d: port base: %04" PRIu32 "\\n", minor, port);
"""
if marker not in text:
    if prep_anchor not in text:
        raise SystemExit(f"ide.c prep anchor not found in {ide_c}")
    text = text.replace(prep_anchor, prep_insert, 1)
    ide_c.write_text(text)
    print(f"ICH7 SATA prep hooked in {ide_c}")
else:
    print(f"ICH7 SATA prep already hooked in {ide_c}")

text = ide_c.read_text()

srst_anchor = """  if (pc386_ide_show)
    printk("IDE%d: port base: %04" PRIu32 "\\n", minor, port);

  outport_byte(port+IDE_REGISTER_DEVICE_HEAD,
               (dev << IDE_REGISTER_DEVICE_HEAD_DEV_POS) | 0xE0);
  wait(10000);
  outport_byte(port+IDE_REGISTER_DEVICE_CONTROL,
               IDE_REGISTER_DEVICE_CONTROL_SRST | IDE_REGISTER_DEVICE_CONTROL_nIEN);
  wait(10000);
  outport_byte(port+IDE_REGISTER_DEVICE_CONTROL,
               IDE_REGISTER_DEVICE_CONTROL_nIEN);
  wait(10000);

  for (dev = 0; dev < 2; dev++)
"""
srst_insert = f"""  if (pc386_ide_show)
    printk("IDE%d: port base: %04" PRIu32 "\\n", minor, port);

  /* {srst_marker} — BIOS already brought up the SATA CF. */
  if (minor != 0) {{
    outport_byte(port+IDE_REGISTER_DEVICE_HEAD,
                 (dev << IDE_REGISTER_DEVICE_HEAD_DEV_POS) | 0xE0);
    wait(10000);
    outport_byte(port+IDE_REGISTER_DEVICE_CONTROL,
                 IDE_REGISTER_DEVICE_CONTROL_SRST | IDE_REGISTER_DEVICE_CONTROL_nIEN);
    wait(10000);
    outport_byte(port+IDE_REGISTER_DEVICE_CONTROL,
                 IDE_REGISTER_DEVICE_CONTROL_nIEN);
    wait(10000);
  }}

  for (dev = 0; dev < 2; dev++)
"""
if srst_marker not in text:
    if srst_anchor not in text:
        raise SystemExit(f"ide.c SRST anchor not found in {ide_c}")
    text = text.replace(srst_anchor, srst_insert, 1)
    ide_c.write_text(text)
    print(f"ICH7 SATA SRST skip hooked in {ide_c}")
else:
    print(f"ICH7 SATA SRST skip already hooked in {ide_c}")
PY
