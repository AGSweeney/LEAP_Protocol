# LeapDevice-linux — Alpine LEAP device for D945GSEJT

Alpine Linux i386 appliance that runs the full LEAP **device** stack on the
Intel D945GSEJT (Atom N270): raw L2 on `eth0` plus LPT1 8×8 digital I/O.

This is an alternative to the RTEMS `leap-port.exe` image in
[`../LeapOS/`](../LeapOS/). Same protocol behavior; Linux kernel drivers for
RTL8111 (`r8169`) and legacy port I/O for the parallel port.

## Build

From WSL:

```bash
cd platforms/x86-32/D945GSEJT/LeapDevice-linux
bash build-leap-device.sh
sudo bash alpine/mk-image.sh          # first time: sudo FORCE_ROOTFS=1
```

Output: `LeapOS/rtems-image/leapos-device-alpine.img`

## Runtime

- Boots to OpenRC → `/usr/sbin/leap-device` (static i386 binary, ~670 KB)
- LEAP EtherType `0x88B6` on first available `eth0`/`eth1`/`eth2`
- LPT1 @ `0x378` — requires root (`iopl`/`ioperm`) for digital I/O
- Serial log on **COM1 @ 115200 8N1**
- No IPv4 required for LEAP (raw L2 only)

## Source layout

| Path | Purpose |
|------|---------|
| `src/device_main_linux.c` | Main recv/tick loop (from LeapPort `init.c`) |
| `src/leap_board_linux.c` | LPT port I/O |
| `src/device_net_linux.c` | NIC up (no IP) |
| `build-leap-device.sh` | Static i386 compile + overlay install |
| `alpine/mk-image.sh` | Alpine rootfs + GRUB ext4 CF image |
| `alpine/make-pxe-device-alpine.sh` | PXE staging (kernel + initramfs + modloop) |

Shared: [`../../../../leap_core/`](../../../../leap_core/) device stack,
[`../LeapGateway-linux/src/leap_transport_linux.c`](../LeapGateway-linux/src/leap_transport_linux.c)
(AF_PACKET transport).

RTEMS reference: [`../LeapPort/`](../LeapPort/)

## QEMU smoke test

```bash
qemu-system-i386 -m 256 -drive file=../../LeapOS/rtems-image/leapos-device-alpine.img,format=raw,if=ide \
  -serial mon:stdio -nographic
```

LEAP traffic requires a host-side controller on a bridged/tapped interface;
raw L2 inside QEMU user networking will not reach an external NIC.

See [`alpine/README.md`](alpine/README.md) for host package setup.
