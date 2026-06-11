# Raspberry Pi 4 — LeapOS-Gateway

Gateway-only Linux target for **Raspberry Pi 4**, **Pi 400**, and **CM4** (BCM2711,
aarch64). Uses the onboard **Gigabit Ethernet** (`eth0`, `bcmgenet` driver) for
plant/cell traffic and ships the same key/value config model as the x86-32 gateway.

## Board

| Item | Detail |
| --- | --- |
| SoC | Broadcom BCM2711 (Cortex-A72, aarch64) |
| Ethernet | Onboard RJ45 (bcmgenet) |
| Storage | microSD (raw `.img` written with Etcher or `dd`) |
| Console | GPIO UART (`serial0`) @ 115200 8N1 — enable `enable_uart=1` in firmware config |

## Status

**Scaffold.** Alpine aarch64 image build, OpenRC stub daemon, and static LEAP
AF_PACKET tools. Full LEAP/EIP/Web gateway binary is ported from
[x86-32 LeapGateway](../../x86-32/D945GSEJT/LeapGateway/) in a follow-up.

## Project layout

| Path | Purpose |
| --- | --- |
| [LeapGateway/](LeapGateway/) | Gateway product notes + future native build |
| [LeapGateway-linux/alpine/](LeapGateway-linux/alpine/) | Alpine aarch64 SD card image builder |
| `image/` | Generated `leapos-gateway-alpine.img` (gitignored) |

## Build image

From WSL or native Linux (needs root for chroot):

```bash
cd platforms/raspberry-pi/Pi4/LeapGateway-linux/alpine
sed -i 's/\r$//' mk-image.sh
sudo bash mk-image.sh
```

Optional — compile static aarch64 LEAP tools into the overlay first:

```bash
bash build-leap-tools.sh
sudo bash mk-image.sh
```

Output: `../image/leapos-gateway-alpine.img`

## Flash and boot

1. Write the raw image to microSD (`>= 512 MiB` card; image default is 512 MiB).
2. Insert SD, connect Ethernet, power on.
3. Serial on GPIO 14/15 (or USB-serial adapter on the UART pins) @ 115200.
4. Log in as `root` (no password on first boot).

`rc-status` should show `leap-gateway [started]`. Check networking:

```sh
ip addr show eth0
```

## Configuration

Persisted at `/cf/config.txt` on the root filesystem (same keys as x86 gateway):

```
network.mode=single
network.ifname=eth0
network.ipv4=192.168.1.2
network.mask=255.255.255.0
network.dhcp=0
```

## See also

- [LeapGateway/README.md](LeapGateway/README.md) — product role and REST API (planned)
- [x86-32 D945GSEJT gateway](../../x86-32/D945GSEJT/LeapGateway-linux/alpine/README.md) — reference Alpine appliance
- [Linux loopback tools](../../../examples/linux_loopback/README.md)
