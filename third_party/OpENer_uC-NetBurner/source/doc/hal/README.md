# OpENer_uC-NetBurner — embedded HAL

The Hardware Abstraction Layer is the **only** boundary between the portable EtherNet/IP core and the NetBurner NNDK environment in **OpENer_uC-NetBurner**.

## Modules

| Header | Responsibility |
|--------|----------------|
| [`opener_hal_types.h`](opener_hal_types.h) | Shared handles, timestamps, zero-copy buffer views |
| [`opener_timer_hal.h`](opener_timer_hal.h) | Microsecond monotonic clock (ACD, connection timers) |
| [`opener_net_hal.h`](opener_net_hal.h) | TCP/UDP sockets, interface config, hostname |
| [`opener_mem_hal.h`](opener_mem_hal.h) | Static pool allocator backing `CipCalloc` / `CipFree` |

## Implementation strategy

1. **NetBurner (production):** [`source/src/ports/netburner/`](../ports/netburner/) — NNDK-native TCP/UDP, timers, ifconfig. Select with `-DOPENER_NET_BACKEND=netburner -DOPENER_NNDK_ROOT=C:/nburn`.
2. **Stub (dev-only):** [`opener_hal_stub.c`](opener_hal_stub.c) — compiles but returns errors; use with `-DOPENER_BUILD_NETWORK_LAYER=OFF` for host compile checks without NNDK.
3. **Network handler:** [`generic_networkhandler.c`](../ports/generic_networkhandler.c) calls `OpenerHal_*` directly — no BSD socket shims in the core.
4. **Legacy adapter:** [`opener_hal_adapters.c`](opener_hal_adapters.c) maps `GetMilliSeconds()` / platform close hooks to the HAL.

## Zero-copy UDP (optional)

Implement `OpenerHal_UdpRecvZeroCopy()` and `OpenerHal_BufferRelease()` to expose NNDK buffer memory directly to the stack. When unsupported, fall back to `OpenerHal_UdpRecv()`.

## Timer requirements

`OpenerHal_TimerGetMicroseconds()` must use a **monotonic** clock. NetBurner default tick is ~50 ms; ACD may need `HalGetTickFraction()` or a hardware timer — see `opener_hal_timer_netburner.c`.

## CMake

```cmake
-DOPENER_NET_BACKEND=netburner
-DOPENER_NNDK_ROOT=C:/nburn
-DOPENER_HAL_MEM_POOL_SIZE=32768
-DOPENER_BUILD_NETWORK_LAYER=ON
```

Dev-only (no NNDK):

```cmake
-DOPENER_NET_BACKEND=stub
-DOPENER_BUILD_NETWORK_LAYER=OFF
```
