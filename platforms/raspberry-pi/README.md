# Raspberry Pi

LEAP **gateway** ports for Raspberry Pi boards running Linux (Alpine appliance images).

Host-side controllers and conformance tools use repo-root CMake on Windows or Linux —
see [docs/BUILD.md](../../docs/BUILD.md).

## Targets

| Target | Path | Status |
| --- | --- | --- |
| Raspberry Pi 4 / 400 / CM4 | [Pi4/](Pi4/) | scaffold — Alpine aarch64 gateway image |

## Pi 4 quick start

```bash
cd platforms/raspberry-pi/Pi4/LeapGateway-linux/alpine
sed -i 's/\r$//' mk-image.sh build-leap-tools.sh
sudo bash mk-image.sh
# → Pi4/image/leapos-gateway-alpine.img
```

Flash the raw image to an SD card (32 MiB or larger), connect the onboard
Gigabit Ethernet port, and open serial on GPIO UART @ **115200 8N1** (see
[Pi4/README.md](Pi4/README.md)).

Default static IPv4: `192.168.1.2/24` on `eth0` (`/cf/config.txt`).
