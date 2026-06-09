# LeapOS Target Hardware

## Primary board

| Component | Detail |
|-----------|--------|
| Board | Intel **D945GSEJT** (Johnstown, Mini-ITX) |
| CPU | Intel **Atom N270** @ 1.6 GHz (**32-bit only**, no x86-64) |
| Chipset | Intel 945GSE + ICH7 |
| RAM | DDR2 SO-DIMM, **max 2 GB** |
| Boot media | **CF via IDE** (CF-to-IDE adapter on parallel ATA) |
| Expansion | 1× PCI 2.3, 2× SATA, 8× USB 2.0 |
| Serial | COM1/COM2 headers (RS-232) |

## Network (important for future work)

| Port | Hardware | RTEMS pc386 status |
|------|----------|-------------------|
| Onboard | **Realtek RTL8111D/8111DL** (PCIe Gigabit) | **Not supported** by legacy pc386 drivers |
| PCI slot | Intel e100 / e1000 recommended | Best path for RTEMS networking |

Do **not** assume the onboard Realtek works with the current PoC or bare `pc386` BSP.
Plan **rtems-libbsd** + Realtek driver investigation, or a **PCI Intel NIC** for bring-up.

## Boot media

User flashes **`leapos-rtems-poc.img`** (preferred) or **`leapos-rtems-poc.iso`**
with **balenaEtcher** or `dd` to CF.
This is appropriate — GRUB and RTEMS BSP messages appear on hardware, confirming
the image was written correctly.

BIOS settings to verify:

- Primary IDE / CF as first boot device
- Legacy BIOS boot (not UEFI-only)
- LBA mode for CF
- Disable TCO/watchdog if available

## Expected PoC boot behavior

### QEMU (verified working)

```
=== LeapOS booting (D945GSEJT / Atom N270) ===
*** LeapOS LEAP device (D945GSEJT, LPT 8x8 I/O) ***
LEAP full stack listening on re0 (DISC/DIR/MGMT/PD/DIAG)
```

### VGA on D945GSE (945GSE IGP)

The onboard Intel 945GSE graphics **does not implement VBE Core** in a way RTEMS
`FB_VESA_RM` can use. Booting with bare `multiboot /leap-port.exe` produces:

```text
FB_VESA_RM VBE Core not implemented!
FB_VESA_RM not initialized, no video selected
```

**Fix:** skip VBE and use VGA **text** mode:

```text
multiboot /leap-port.exe --video=off --console=/dev/vgacons --printk=/dev/vgacons --disable-com1-com4
```

The serial COM1 @ 115200 entry is the **default**; a VGA text entry is also provided
for monitor-only bring-up.

RTEMS 6.2 cannot disable `USE_VBE_RM` at BSP build time without enabling
`USE_VGA`, which currently fails to compile — runtime `--video=off` is required.

### Real hardware (current status — partial)

User reports on D945GSEJT + CF:

1. **Several reboot loops** regardless of GRUB menu choice
2. **Eventually stops** on headless entry (reboots cease)
3. VGA path shows:
   - `FB_VESA_RM VBE Core not implemented!`
   - `FB_VESA_RM not initialized, no video selected`
   - `Console: /dev/vgacons printk: /dev/vgacons`
   - `i386: isr=0 irr=1`
4. **Does not consistently reach** the LeapOS banner on VGA

### Diagnosis

| Observation | Meaning |
|-------------|---------|
| GRUB menu appears | Image boot OK |
| BSP printk lines appear | RTEMS `bsp_start()` running |
| `i386: isr=0 irr=1` | Last line of `bsp_start()` — timer IRQ enabled (`Clock_driver_install_handler`) |
| Reboot before LeapOS banner | Crash/triple-fault during executive init, not intentional shutdown |
| Intermittent loops | Timing/hardware-specific; Atom/945GSE is "partial support" per pc386 README |
| Headless eventually stops | VGA avoided; may still **hang** rather than succeed |

Crash location in source tree:

- `bsps/i386/pc386/start/bspstart.c` — `Clock_driver_install_handler()` is final call
- `bsps/i386/shared/irq/irq.c` — prints `i386: isr=%x irr=%x` when enabling IRQ 0

### Recommended hardware bring-up procedure

1. Connect **USB-serial to COM1**: **115200 8N1**
2. Boot GRUB entry: **"LeapOS Device (serial COM1 @ 115200)"**
3. Or manual GRUB:
   ```
   multiboot /leap-port.exe --video=off --console=/dev/com1,115200 --printk=/dev/com1,115200
   boot
   ```
4. Determine whether serial shows the LeapOS banner or stops at `i386: isr=0 irr=1`

### Suspected causes (priority order)

1. **VGA/VESA console failure** on 945GSE — use serial only
2. **ICH7 TCO watchdog** — `bsp_start()` has empty watchdog-disable stub
3. **PIC/IRQ quirks** on Atom vs QEMU — intermittent timer IRQ issues
4. **Not a flash problem** — balenaEtcher + CF is fine given BSP output is visible

## Console reference

| Console | Device string | Use case |
|---------|---------------|----------|
| Serial COM1 | `/dev/com1` | Headless bring-up (recommended) |
| VGA text | `/dev/vgacons` | **Default** — use with `--video=off` on 945GSE |
| Video off | `--video=off` | Required on 945GSE to skip broken VBE init |

## Future hardware plan (LEAP gateway)

```
eth0 (PCI Intel NIC)  →  LEAP raw L2 (isolated cell, no IP)
eth1 (onboard RTL8111D) →  Plant network (needs libbsd + driver)
        — OR swap roles once Realtek driver is working
COM1                  →  Modbus RTU (optional)
```

Target LEAP cyclic period on N270: **50–100 ms** initially.
