# OpENer_uC-NetBurner — NetBurner HAL backend

Native `OpenerHal_*` implementation for NetBurner NNDK in **OpENer_uC-NetBurner** — no BSD socket macro shims.

## CMake

```cmake
-DOPENER_NET_BACKEND=netburner
-DOPENER_NNDK_ROOT=C:/nburn
-DOPENER_BUILD_NETWORK_LAYER=ON
-DOPENER_LLDP=ON          # Extended tier
-DOPENER_NB_LLDP=ON       # Extended tier wire LLDP
-DOPENER_NB_ACD=ON        # Extended tier ACD
```

Or use `.\scripts\build-nb.ps1 -Profile Extended` from repo root.

Link the resulting OpENer static libraries with your NetBurner firmware project (nbrtos, platform startup, etc.).

## Interface selection

Before `NetworkHandlerInitialize()`, call:

```c
OpenerHal_NetInit((OpenerNetIfHandle)(intptr_t)ifnum);
```

NetBurner interface numbers are **1-based**. Default is `1` when `OpenerHal_NetInit(NULL)` is used.

## API mapping

| OpenerHal | NetBurner NNDK |
|-----------|----------------|
| `OpenerHal_TcpListen` | `listenvia4(NullIP, port, InterfaceIP(ifnum), backlog)` |
| `OpenerHal_TcpAccept` | `accept4(fd, &IPADDR4, &port, 0)` |
| `OpenerHal_UdpOpen` | `CreateRxTxUdpSocketVia4(NullIP, 0, port, ifnum)` |
| `OpenerHal_UdpSend` / `UdpRecv` | `sendto4` / `recvfrom4` |
| `OpenerHal_SocketPoll` | `select(nfds, &fds, NULL, NULL, ticks)` |
| `OpenerHal_GetPeer` | Peer table (UDP `getpeername` is unreliable) |
| `OpenerHal_TimerGetMicroseconds` | `TimeTick` + `HalGetTickFraction()` |

Explicit UDP unicast and broadcast share one socket on NetBurner (same as the MODM7AE70 reference port).

## Files

| File | Role |
|------|------|
| `opener_hal_netburner.cpp` | TCP/UDP/poll/interface HAL |
| `opener_hal_timer_netburner.c` | Monotonic timer |
| `opener_nb_ifconfig.cpp` | MAC/IP/DNS, hostname, PHY link → CIP |
| `opener_nb_config.h` | Interface handle helpers |
