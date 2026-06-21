# LeapDevice-linux Driver Scaffolds

These files are not wired into `build-leap-device.sh` yet. They are staged here
so hardware-specific board support can be integrated without changing the LEAP
device stack.

## Measurement Computing PCI-DIO-24H

Files:

- `mcc_pci_dio24h.h`
- `mcc_pci_dio24h.c`

Purpose: userspace Linux scaffold for the Measurement Computing / ComputerBoards
PCI-DIO-24H 24-channel digital I/O board.

Known hardware assumptions from Linux Comedi references:

- PCI vendor ID: `0x1307`
- PCI device ID: `0x0014` (PCI-DIO-24H) or `0x0028` (PCI-DIO-24 / Rev 02)
- One Intel 8255-compatible DIO block
- 8255 DIO registers exposed through PCI BAR/resource index `2`
- Only 8255 mode 0 is planned for LEAP use

Exposed scaffold surface:

- Port A: channels 0-7
- Port B: channels 8-15
- Port C: channels 16-23
- Packed 24-bit read/write helpers for all channels
- Direction control follows 8255 mode 0 groups: Port A, Port B, Port C low,
  and Port C high

There is intentionally no LEAP profile mapping in this scaffold. The
PCI-DIO-24H is a 24-channel card and should get a real 24-channel integration
path when the hardware arrives.

First hardware bring-up checklist:

1. Confirm PCI enumeration:

   ```sh
   lspci -nn | grep -iE '1307:0014|1307:0028'
   ```

2. Confirm BAR/resource layout:

   ```sh
   for d in /sys/bus/pci/devices/*; do
     [ "$(cat "$d/vendor" 2>/dev/null)" = "0x1307" ] || continue
     [ "$(cat "$d/device" 2>/dev/null)" = "0x0014" ] || continue
     echo "$d"
     cat "$d/resource"
   done
   ```

3. Verify the DIO base is an I/O port resource at resource index 2.
4. Compile the scaffold manually or wire it into a temporary board shim.
5. Confirm whether the shipped profile should be 24 inputs, 24 outputs, or a
   mixed 8255 group layout.
6. Validate safe outputs on boot, owner release, watchdog expiry, and daemon exit.

Integration note:

The current Alpine device port uses `src/leap_board_linux.c` for LPT1 8x8 I/O.
When the PCI-DIO-24H hardware arrives, add a board-selection/profile layer
rather than replacing the LPT file in-place, so the existing D945 LPT behavior
remains available for regression tests and the PCI card can expose all 24
channels.
