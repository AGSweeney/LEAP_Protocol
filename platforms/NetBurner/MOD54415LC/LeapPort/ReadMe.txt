/** @page leapNetBurnerScaffold LEAP NetBurner Scaffold

# LEAP NetBurner Scaffold

## Overview

This project is scaffolded for LEAP raw Ethernet transport on NetBurner (MOD54415LC).  
It replaces the prior TCP multi-socket example loop with a LEAP runtime skeleton.

## Transport Model

- LEAP transport is raw L2 Ethernet (not TCP/UDP sockets)
- Default development EtherType is `0x88B6`
- Alternate private-test EtherType is `0x88B5`

## Current Scaffold Status

The following pieces are wired and ready for implementation:

- System + network bring-up in `UserMain()`
- LEAP runtime initialization (`leap_runtime_init`)
- Poll loop (`leap_runtime_poll`)
- Transport API stubs:
  - `leap_transport_init`
  - `leap_transport_receive`
  - `leap_transport_send`
- Frame dispatch hook (`leap_runtime_dispatch_frame`)

## Files

- `src/main.cpp`: LEAP runtime/task loop + transport stubs
- `src/leap_config.h`: EtherType and frame-size constants
- `src/leap_transport.h`: transport-facing data types and API
- `src/leap_runtime.h`: runtime state and poll/init API

## Next Steps

1. Implement raw Ethernet RX/TX open and EtherType filtering in `leap_transport_init`
2. Parse incoming Ethernet frames in `leap_transport_receive`
3. Add LEAP header/CRC validation and service dispatch in `leap_runtime_dispatch_frame`
4. Add LEAP reply path via `leap_transport_send`
5. Add stats/diagnostics and link-monitor reconnect policy

## Notes

- The scaffold intentionally keeps protocol handlers out of transport code.
- LEAP frame parsing and service routing belong in runtime/service layer code.
- Keep this scaffold as a minimal bring-up baseline before pulling in full stack services.

*/

