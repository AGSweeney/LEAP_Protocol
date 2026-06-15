# STM32F746G-DISCO

STM32CubeIDE project for the STM32746G-Discovery board.

Import this directory in STM32CubeIDE:

    platforms/STM/STM32F746G-DISCO

Firmware brings up RMII Ethernet and the LwIP stack (DHCP with static fallback), then runs a **LEAP device** on raw L2 (`0x88B5`).

## LEAP I/O profile

| Field | Value |
| --- | --- |
| Profile | `LEAP_PROFILE_DIGITAL_IO_8X8` (`0x00010001`) |
| Outputs | 8 bits (simulated shadow) |
| Inputs | 8 bits (simulated shadow) |
| I/O model | **Simulated** — no GPIO; inputs mirror outputs (`inputs = outputs & 0xFF`) |

`leap_dir_device_config_set_digital_io()` advertises the 8×8 profile in LEAP-DIR. PD EXCHANGE drives the output shadow; `leap_hw_refresh_inputs()` copies those bits into the input shadow before each PD frame is processed.

## Source layout

| Path | Role |
| --- | --- |
| `Src/leap_device/` | Board config, simulated I/O, LwIP hook, LEAP host |
| `../../../../leap_core/` | Shared LEAP stack (linked in `.project`) |

After changing linked sources or include paths, refresh the project in CubeIDE (F5) and rebuild.

## Locate

LEAP-DISC locate blinks **LED1** on the Discovery board.
