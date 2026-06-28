# Memory pool

Static pool backs `CipCalloc` / `CipFree` on embedded builds (`OpenerHal_MemCalloc` / `OpenerHal_MemFree`).

Default size: **32 KiB** (`OPENER_HAL_MEM_POOL_SIZE` in CMake). Increase if Forward Open or File Object init fails:

```cmake
-DOPENER_HAL_MEM_POOL_SIZE=65536
```

Implementation: [`hal/opener_mem.c`](../src/hal/opener_mem.c), [`hal/opener_cip_alloc.c`](../src/hal/opener_cip_alloc.c).

Heavy init-time consumers: `cipcommon.c`, `cipassembly.c`, connection manager, optional File Object.

Optional later: zero-copy UDP via `OpenerHal_UdpRecvZeroCopy()` — not wired in `generic_networkhandler.c` yet.
