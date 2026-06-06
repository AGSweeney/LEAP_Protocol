# Espressif

LEAP device ports for Espressif hardware. Firmware builds with **ESP-IDF** inside
each platform folder (`idf.py build`). Windows/Linux host controllers for bench tests
use repo-root CMake — [docs/BUILD.md](../../docs/BUILD.md).

| Target | Path | Status |
| --- | --- | --- |
| GL-C-618WL (Gledopto ESP32 + LAN8720) | [GL-C-618WL/](GL-C-618WL/) | scaffold — ESP-IDF LEAP device |
| KC868-A16 (Kincony ESP32 + LAN8720) | [Kincony/KC868-A16/](Kincony/KC868-A16/) | scaffold — ESP-IDF LEAP device |
| ESP32 | [ESP32/](ESP32/) | placeholder |
| ESP32-P4 | [ESP32-P4/](ESP32-P4/) | placeholder |

## GL-C-618WL quick start

```bash
cd platforms/Espressif/GL-C-618WL
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

See [GL-C-618WL/README.md](GL-C-618WL/README.md) for hardware pinout, PD I/O
mapping, and controller test commands.

## KC868-A16 quick start

```bash
cd platforms/Espressif/Kincony/KC868-A16
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

See [Kincony/KC868-A16/README.md](Kincony/KC868-A16/README.md) for hardware
pinout and PD I/O mapping.
