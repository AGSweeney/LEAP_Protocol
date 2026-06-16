# D945GSEJT

LEAP device port for the Intel **D945GSEJT** embedded Mini-ITX board, packaged as
**LeapOS** — a bootable RTEMS (pc386 BSP) image that turns the board into a LEAP
field device. It speaks the full LEAP protocol (DISC/DIR/MGMT/PD/DIAG) over raw
Layer 2 Ethernet on the onboard NIC and drives digital I/O on the board's parallel
(LPT) port: **8 outputs and 5 inputs** presented as the LEAP 8×8 profile.

## Board

| Item | Detail |
| --- | --- |
| CPU | Intel Atom N270 (x86-32, single core) |
| Chipset | Intel 945GSE + ICH7 |
| Ethernet | Realtek RTL8111D/8111DL (PCIe Gigabit) |
| Form factor | Mini-ITX / embedded ATX |
| Typical use | Lab LEAP conformance fleet (~60 boards) |

The onboard Realtek NIC is the primary LEAP transport interface. On RTEMS libbsd
this typically binds as **`re0`** (FreeBSD `re` / Realtek 8168/8111 family driver).
No IP stack is required for protocol traffic (EtherType `0x88B6`).

## Status

**Working.** Boots to a live LEAP device and passes Conformance Studio /
`leap_conformance` cyclic exchange.

What runs today:

- libbsd brings up the onboard `re0` NIC; raw L2 transport via BPF on EtherType `0x88B6` (no IP stack).
- Full `leap_device_stack`: discovery, directory, management, process data, and diagnostics.
- Digital I/O on the LPT port — 8 outputs and 5 inputs (see [I/O wiring](#io-wiring)).
- Colorized, timestamped boot log on serial COM1 @ 115200 8N1.

## Project layout

| Path | Purpose |
| --- | --- |
| [LeapOS/](LeapOS/) | RTEMS boot image build (LeapPort device) |
| [LeapPort/](LeapPort/) | LEAP device stack RTEMS application |
| [LeapDevice-linux/](LeapDevice-linux/) | LEAP device product (Alpine Linux i386) |
| [LeapGateway/](LeapGateway/) | Shared gateway application sources |
| [LeapGateway-linux/](LeapGateway-linux/) | LEAP gateway product (Alpine Linux i386) |
| `LeapPort/src/init.c` | RTEMS `Init` task, device stack main loop |
| `LeapPort/src/leap_transport.*` | libbsd raw Ethernet (BPF) on `re0` |
| `LeapPort/src/leap_board.*` | LPT-port digital I/O (8 out / 5 in) |
| `LeapPort/src/leap_time.*` | Monotonic microseconds for DIAG/tick |
| `LeapPort/generated/` | `leap_build_info_gen.h` (configure-time) |
| `../../../../leap_core/` | Linked reference stack sources (via wscript) |

## Boot image (LeapOS)

Build a bootable CF/IDE or USB image before LeapPort is integrated:

```bash
cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
bash build-all.sh
# → rtems-image/leapos-device.img + leapos-device.iso (RTEMS)
```

Alpine Linux device (alternative):

```bash
cd platforms/x86-32/D945GSEJT/LeapDevice-linux
bash build-leap-device.sh && sudo bash alpine/mk-image.sh
# → rtems-image/leapos-device-alpine.img
```

Gateway (Alpine Linux): [LeapGateway-linux/](LeapGateway-linux/) → `leapos-gateway-alpine.img`.

Full instructions: [LeapOS/docs/BUILD.md](LeapOS/docs/BUILD.md).

- [RTEMS 6.x](https://docs.rtems.org/) tool chain built for **i386-rtems6**
- **pc386** BSP (generic PC; D945GSEJT is pc386-class hardware)
- libbsd enabled in the BSP (`--enable-libbsd` when building RTEMS)
- **Realtek `re` driver** included in the pc386/libbsd build (RTL8111D/DL on D945GSEJT)
- Serial console (115200 8N1 typical) for boot logs
- LEAP master on the same L2 segment (Studio or `leap_win_controller`)

## Build (outline)

```bash
cd platforms/x86-32/D945GSEJT/LeapPort

# One-time: generate leap_build_info_gen.h (or run repo cmake configure)
./scripts/gen_build_info.sh

export RTEMS_TOOLS=/path/to/rtems6/bin
export RTEMS_BSPS=/path/to/rtems6/i386-rtems6

./waf configure --rtems-bsp=pc386/pc386 --rtems-version=6
./waf
```

Flash or PXE-boot the resulting `build/i386-rtems6-pc386-pc386/bin/leap_d945gsejt.exe`
onto the target (see your RTEMS pc386 boot media setup).

## Runtime

On boot the firmware should:

1. Initialize libbsd and bring up the onboard NIC
2. Open raw L2 transport filtered to LEAP EtherType
3. Run `leap_device_stack` recv loop + monotonic tick for lease/watchdog
4. Log link and MGMT transitions on serial (errors by default)

Test from Windows (Administrator, Npcap):

```text
leap_win_controller.exe --outputs 0x0001 <adapter>
```

Or run **device_conformance** in LEAP Conformance Studio once transport is live.

## I/O wiring

The LPT1 port at base `0x378` provides **8 outputs and 5 physical inputs** (set the
BIOS parallel port to SPP / bi-directional). Pin numbers below are the **D945GSEJT
on-board 26-pin parallel header** (Table 19, Standard/SPP). The LEAP 8×8 profile
(`LEAP_PROFILE_DIGITAL_IO_8X8`) is filled by mirroring 3 outputs into the spare
input bits:

| LEAP | Signal | Header pin |
|------|--------|------------|
| D0 | PD0 | 3 |
| D1 | PD1 | 5 |
| D2 | PD2 | 7 |
| D3 | PD3 | 9 |
| D4 | PD4 | 11 |
| D5 | PD5 | 13 |
| D6 | PD6 | 15 |
| D7 | PD7 | 17 |
| in0 | ACK# | 19 |
| in1 | BUSY | 21 |
| in2 | PERROR | 23 |
| in3 | SELECT | 25 |
| in4 | FAULT# | 4 |
| in5 | (mirrored D0) | 3 |
| in6 | (mirrored D1) | 5 |
| in7 | (mirrored D2) | 7 |

- **Inputs 5–7** are software mirrors of D0–D2 (not physical reads; keeps data lines output-only).
- **Ground:** even header pins 10, 12, 14, 16, 18, 20, 22, 24.

## Notes

- Port follows the **stack-only** path (`leap_device_stack`), not hand-rolled
  MGMT/PD logic. See [docs/README.md](../../../docs/README.md) porting gate.
- I/O bench DIAG polling checkbox in Studio applies here as on NetBurner — disable
  during tight cyclic soak if parallel DIAG traffic adds jitter.

## See also

- [x86-32 platforms index](../README.md)
- [NetBurner MOD54415LC](../../NetBurner/MOD54415LC/README.md) — reference
  embedded port with full conformance pass
- [Linux loopback templates](../../../examples/linux_loopback/README.md)
