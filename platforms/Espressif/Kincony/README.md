# Kincony

LEAP device ports on Kincony hardware.

| Target | Path | Status |
| --- | --- | --- |
| KC868-A16 | [KC868-A16/](KC868-A16/) | scaffold — ESP-IDF LEAP device |

## KC868-A16 quick start

```bash
cd platforms/Espressif/Kincony/KC868-A16
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

See [KC868-A16/README.md](KC868-A16/README.md) for hardware pinout, PD I/O
mapping, and controller test commands.
