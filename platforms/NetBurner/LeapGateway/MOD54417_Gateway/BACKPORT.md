# OpENer backport notes (NetBurner -> LeapOS-Gateway Alpine i386)

The **MOD54417_Gateway** embedded port carries OpENer enhancements that are **not yet** in
[`platforms/x86-32/D945GSEJT/LeapGateway-linux/`](../../../../x86-32/D945GSEJT/LeapGateway-linux/).
When stabilised on hardware, backport these to the Alpine `leap-gateway` binary.

## NetBurner-only OpENer features (backport candidates)

| Feature | Location | Notes |
|---------|----------|-------|
| Identity lifecycle (Self-Testing / Standby / fault flags) | `opener/netburner_port/leap_gateway/leapgateway.c` | FusionCore-style `IdentityEnter`, I/O activity on assemblies 100/150 |
| Micro800 run-idle compat (O2T/T2O headers + Forward Open/receive strip) | `patch-opener-netburner.ps1`, `build-opener-netburner.ps1`, `leapgateway.c` | Patched into staged OpENer-Enhanced; `OpenerConfigureMicro800RunIdleCompat()` at init |
| TCP/IP attr 5 live gateway + DNS read | `netburner_port/networkconfig.c`, `nb_ifconfig.cpp` | Reads `InterfaceGate`, `InterfaceDNS` from NNDK |
| Settable TCP/IP + NVRAM persist | `nb_nvtcpip.cpp`, `OPENER_TCPIP_IFACE_CFG_SETTABLE` | Molex workflow: Static -> attr 5 -> reset |
| Connection Manager statistics (attr 8) | `cipconnectionmanager_stats.c` + patch script | Real counters vs stub |
| Ethernet Link real counters + interface state | `ethlinkcbs.c`, `nb_eth_counters.c` | Attr 8 link up/down; boot `ApplicationNotifyLinkUp` |
| Reset type 0 = reboot only; type 1 = factory DHCP + QoS NV | `leapgateway.c` | Differs from minimal Alpine reset |
| Deferred reboot | `nb_reboot.cpp` | 500 ms delay before `ForceReboot()` |
| Raw EtherType RX hook (`CustomNetDoRX`) | `overload/nbrtos/source/netrx.cpp` | Required for LEAP L2 on Port 2; copy from `MOD54415LC/LeapPort` |
| List Identity IP endian fix | `patch-opener-netburner.ps1` | `EncapsulateIpAddress` |

## Shared LeapOS-Gateway logic (now linked on NetBurner)

Embedded firmware links the same sources as Alpine:

- `platforms/x86-32/D945GSEJT/LeapGateway/src/` - `gateway_global`, `gateway_leap_session`, `gateway_pd_io`, `gateway_rtems_io`, `leap_gateway_opener`
- `leap_core/` - `leap_eip_bridge`, session hub, PD controller (gateway subset)

Build: `build-leap-gateway-netburner.ps1` -> `leap/build-work/libleap_gateway_netburner.a`

## Backport workflow (suggested)

1. Copy `leapgateway.c` identity/reset/Eth Link patterns into Alpine `sampleapplication.c` or shared hook file.
2. Port `nb_nvtcpip` / settable TCP/IP gate to Linux `networkconfig.c` + persistent store under `/cf`.
3. Merge CM stats + Eth Link counter modules into `LeapGateway-linux/opener/` overlay.
4. Re-run `build-leap-gateway.sh` and regression-test on D945GSEJT / QEMU Alpine image.
