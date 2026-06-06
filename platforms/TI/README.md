# Texas Instruments

LEAP device ports for TI hardware. Firmware builds **inside each CCS or BeagleBone
project** — not in the repo-root CMake trees. Windows/Linux host tools for testing
use [docs/BUILD.md](../../docs/BUILD.md).

## Targets

| Target | Path | Status |
| --- | --- | --- |
| BeagleBone Black / Green (AM335x) | [BeagleBone/](BeagleBone/) | Active |
| LP-AM243 | [LP-AM243/](LP-AM243/) | Active |

## BeagleBone quick links

- Build runtime: `platforms/TI/BeagleBone/build_leap_led_device.ps1`
- Runtime source: `platforms/TI/BeagleBone/leap_led_device/`
- Boot support: `platforms/TI/BeagleBone/boot/`
- Usage + pinout: `platforms/TI/BeagleBone/README.md`
